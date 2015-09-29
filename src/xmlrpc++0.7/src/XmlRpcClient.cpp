
#include "XmlRpcClient.h"

#include "XmlRpcSocket.h"
#include "XmlRpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace XmlRpc;
using namespace xmlrpc;

// Static data
const char XmlRpcClient::REQUEST_BEGIN[] =
  "<?xml version=\"1.0\"?>\r\n"
  "<methodCall><methodName>";
const char XmlRpcClient::REQUEST_END_METHODNAME[] = "</methodName>\r\n";
const char XmlRpcClient::PARAMS_TAG[] = "<params>";
const char XmlRpcClient::PARAMS_ETAG[] = "</params>";
const char XmlRpcClient::PARAM_TAG[] = "<param>";
const char XmlRpcClient::PARAM_ETAG[] =  "</param>";
const char XmlRpcClient::REQUEST_END[] = "</methodCall>\r\n";
const char XmlRpcClient::METHODRESPONSE_TAG[] = "<methodResponse>";
const char XmlRpcClient::FAULT_TAG[] = "<fault>";



XmlRpcClient::XmlRpcClient(const char* host, int port, const char* uri/*=0*/)
{
    LOG_DEBUG("New client: host " << host << " port " << port);

  _host = host;
  _port = port;
  if (uri)
    _uri = uri;
  else
    _uri = "/RPC2";
  _connectionState = NO_CONNECTION;
  _executing = false;
  _eof = false;

  // Default to keeping the connection open until an explicit close is done
  setKeepOpen();
}


XmlRpcClient::~XmlRpcClient()
{
    try
    {
        close();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to close: " << e.what());
    }
    catch (...)
    {
        LOG_ERROR("Failed to close: unknown exception");
    }
}

// Close the owned fd
void
XmlRpcClient::close()
{
    LOG_DEBUG( "fd " <<  getfd());
  _connectionState = NO_CONNECTION;
  _disp.exit();
  _disp.removeSource(this);
  XmlRpcSource::close();
}


// Clear the referenced flag even if exceptions or errors occur.
struct ClearFlagOnExit {
  ClearFlagOnExit(bool& flag) : _flag(flag) {}
  ~ClearFlagOnExit() { _flag = false; }
  bool& _flag;
};

// Execute the named procedure on the remote server.
// Params should be an array of the arguments for the method.
// Returns true if the request was sent and a result received (although the result
// might be a fault).
bool
XmlRpcClient::execute(const char* method,
                      XmlRpcValue const& params,
                      XmlRpcValue& result,
                      double msTime)
{
    LOG_DEBUG("method " << method << "(_connectionState " <<  _connectionState << ")");

  // This is not a thread-safe operation, if you want to do multithreading, use separate
  // clients for each thread. If you want to protect yourself from multiple threads
  // accessing the same client, replace this code with a real mutex.
  if (_executing)
    return false;

  _executing = true;
  ClearFlagOnExit cf(_executing);

  _sendAttempts = 0;
  _isFault = false;

  if ( ! setupConnection())
    return false;

  if ( ! generateRequest(method, params))
    return false;

  result.clear();
  //  double msTime = -1.0;   // Process until exit is called
  _disp.work(msTime);

  if (_connectionState != IDLE || ! parseResponse(result))
    return false;

  LOG_DEBUG("method " << method << " completed.");
  _response = "";
  return true;
}

// XmlRpcSource interface implementation
// Handle server responses. Called by the event dispatcher during execute.
unsigned
XmlRpcClient::handleEvent(unsigned eventType)
{
    if (eventType == XmlRpcDispatch::Exception)
    {
        if (_connectionState == WRITE_REQUEST && _bytesWritten == 0)
        {
            LOG_ERROR("Could not connect to server: "
                      << XmlRpcSocket::getErrorMsg());
        }
        else
        {
            LOG_ERROR("Error in XmlRpcClient::handleEvent (state " << _connectionState << "): "
                      << XmlRpcSocket::getErrorMsg());
        }
        return 0;
    }

    if (_connectionState == WRITE_REQUEST)
        if ( ! writeRequest()) return 0;

    if (_connectionState == READ_HEADER)
        if ( ! readHeader()) return 0;

    if (_connectionState == READ_RESPONSE)
        if ( ! readResponse()) return 0;

    // This should probably always ask for Exception events too
    return (_connectionState == WRITE_REQUEST)
        ? XmlRpcDispatch::WritableEvent : XmlRpcDispatch::ReadableEvent;
}


// Create the socket connection to the server if necessary
bool
XmlRpcClient::setupConnection()
{
  // If an error occurred last time through, or if the server closed the connection, close our end
  if ((_connectionState != NO_CONNECTION && _connectionState != IDLE) || _eof)
    close();

  _eof = false;
  if (_connectionState == NO_CONNECTION)
    if (! doConnect())
      return false;

  // Prepare to write the request
  _connectionState = WRITE_REQUEST;
  _bytesWritten = 0;

  // Notify the dispatcher to listen on this source (calls handleEvent when the socket is writable)

  // AR: this invalidates thisIt in XmlRpcDispatch::work() - so don't attempt to
  // remove it but update it if it's already present.
  //
  // _disp.removeSource(this);       // Make sure nothing is left over
  // _disp.addSource(this, XmlRpcDispatch::WritableEvent | XmlRpcDispatch::Exception);

  const unsigned mask = XmlRpcDispatch::WritableEvent bitor XmlRpcDispatch::Exception;
  if (_disp.hasSource(this))
  {
      _disp.setSourceEvents(this, mask);
  }
  else
  {
      _disp.addSource(this, mask);
  }

  return true;
}


// Connect to the xmlrpc server
bool
XmlRpcClient::doConnect()
{
  int fd = XmlRpcSocket::socket();
  if (fd < 0)
  {
      LOG_ERROR("Could not create socket " <<  XmlRpcSocket::getErrorMsg());
    return false;
  }

  LOG_DEBUG("fd " << fd);
  this->setfd(fd);

  // Don't block on connect/reads/writes
  if ( ! XmlRpcSocket::setNonBlocking(fd))
  {
    this->close();
    LOG_ERROR("Could not set socket to non-blocking IO mode " <<  XmlRpcSocket::getErrorMsg());
    return false;
  }

  if ( ! XmlRpcSocket::connect(fd, _host, _port))
  {
    this->close();
    LOG_ERROR("Could not connect to server" << XmlRpcSocket::getErrorMsg());
    return false;
  }

  return true;
}

// Encode the request to call the specified method with the specified parameters into xml
bool
XmlRpcClient::generateRequest(const char* methodName, XmlRpcValue const& params)
{
  std::string body = REQUEST_BEGIN;
  body += methodName;
  body += REQUEST_END_METHODNAME;

  // If params is an array, each element is a separate parameter
  if (params.valid()) {
    body += PARAMS_TAG;
    if (params.getType() == XmlRpcValue::TypeArray)
    {
      for (int i=0; i<params.size(); ++i) {
        body += PARAM_TAG;
        body += params[i].toXml();
        body += PARAM_ETAG;
      }
    }
    else
    {
      body += PARAM_TAG;
      body += params.toXml();
      body += PARAM_ETAG;
    }

    body += PARAMS_ETAG;
  }
  body += REQUEST_END;

  std::string header = generateHeader(body);
  LOG_DEBUG("header is  bytes " <<  header.length()
            << " content-length is " << body.length());

  _request = header + body;
  return true;
}

// Prepend http headers
std::string
XmlRpcClient::generateHeader(std::string const& body)
{
  std::string header =
    "POST " + _uri + " HTTP/1.1\r\n"
    "User-Agent: ";
  header += XMLRPC_VERSION;
  header += "\r\nHost: ";
  header += _host;

  char buff[40];
  sprintf(buff,":%d\r\n", _port);

  header += buff;
  header += "Content-Type: text/xml\r\nContent-length: ";

  sprintf(buff,"%lu\r\n\r\n", body.size());

  return header + buff;
}

bool
XmlRpcClient::writeRequest()
{
  if (_bytesWritten == 0)
      LOG_DEBUG("attempt " << _sendAttempts+1 << ":\n" << _request.c_str());

  // Try to write the request
  if ( ! XmlRpcSocket::nbWrite(this->getfd(), _request, &_bytesWritten)) {
      LOG_ERROR("write error " << XmlRpcSocket::getErrorMsg());
    return false;
  }

  LOG_DEBUG("wrote " <<  _bytesWritten << " of " << _request.length() << " bytes.");


  // Wait for the result
  if (_bytesWritten == int(_request.length())) {
    _header = "";
    _response = "";
    _connectionState = READ_HEADER;
  }
  return true;
}


// Read the header from the response
bool
XmlRpcClient::readHeader()
{
  // Read available data
  if ( ! XmlRpcSocket::nbRead(this->getfd(), _header, &_eof) ||
       (_eof && _header.length() == 0)) {

    // If we haven't read any data yet and this is a keep-alive connection, the server may
    // have timed out, so we try one more time.
    if (getKeepOpen() && _header.length() == 0 && _sendAttempts++ == 0) {
        LOG_DEBUG("Re-trying connection");
        XmlRpcSource::close();
        _connectionState = NO_CONNECTION;
        _eof = false;
        return setupConnection();
    }

    LOG_ERROR("Error while reading header fd " << getfd() << ": " << XmlRpcSocket::getErrorMsg());
    return false;
  }

  LOG_DEBUG("client has read " << _header.length() << " bytes");

  char *hp = (char*)_header.c_str();  // Start of header
  char *ep = hp + _header.length();   // End of string
  char *bp = 0;                       // Start of body
  char *lp = 0;                       // Start of content-length value

  for (char *cp = hp; (bp == 0) && (cp < ep); ++cp) {
    if ((ep - cp > 16) && (strncasecmp(cp, "Content-length: ", 16) == 0))
      lp = cp + 16;
    else if ((ep - cp > 4) && (strncmp(cp, "\r\n\r\n", 4) == 0))
      bp = cp + 4;
    else if ((ep - cp > 2) && (strncmp(cp, "\n\n", 2) == 0))
      bp = cp + 2;
  }

  // If we haven't gotten the entire header yet, return (keep reading)
  if (bp == 0) {
    if (_eof)          // EOF in the middle of a response is an error
    {
      LOG_ERROR("EOF while reading header");
      return false;   // Close the connection
    }

    return true;  // Keep reading
  }

  // Decode content length
  if (lp == 0) {
    LOG_ERROR("Error XmlRpcClient::readHeader: No Content-length specified");
    return false;   // We could try to figure it out by parsing as we read, but for now...
  }

  _contentLength = atoi(lp);
  if (_contentLength <= 0) {
      LOG_ERROR("Invalid Content-length specified " << _contentLength);
    return false;
  }

  LOG_DEBUG("client read content length: "<< _contentLength);

  // Otherwise copy non-header data to response buffer and set state to read response.
  _response = bp;
  _header = "";   // should parse out any interesting bits from the header (connection, etc)...
  _connectionState = READ_RESPONSE;
  return true;    // Continue monitoring this source
}


bool
XmlRpcClient::readResponse()
{
  // If we dont have the entire response yet, read available data
  if (int(_response.length()) < _contentLength) {
    if ( ! XmlRpcSocket::nbRead(this->getfd(), _response, &_eof)) {
        LOG_ERROR("read error " << XmlRpcSocket::getErrorMsg());
      return false;
    }

    // If we haven't gotten the entire _response yet, return (keep reading)
    if (int(_response.length()) < _contentLength) {
      if (_eof) {
        LOG_ERROR("EOF while reading response");
        return false;
      }
      return true;
    }
  }

  // Otherwise, parse and return the result
  LOG_DEBUG("Read " << _response.length() << " bytes");
  LOG_DEBUG("response:\n" << _response);

  _connectionState = IDLE;

  return false;    // Stop monitoring this source (causes return from work)
}


// Convert the response xml into a result value
bool
XmlRpcClient::parseResponse(XmlRpcValue& result)
{
  // Parse response xml into result
  int offset = 0;
  if ( ! XmlRpcUtil::findTag(METHODRESPONSE_TAG,_response,&offset)) {
      LOG_ERROR("Invalid response - no methodResponse. Response:\n" << _response);
    return false;
  }

  // Expect either <params><param>... or <fault>...
  if ((XmlRpcUtil::nextTagIs(PARAMS_TAG,_response,&offset) &&
       XmlRpcUtil::nextTagIs(PARAM_TAG,_response,&offset)) ||
      (XmlRpcUtil::nextTagIs(FAULT_TAG,_response,&offset) && (_isFault = true)))
  {
    if ( ! result.fromXml(_response, &offset)) {
        LOG_ERROR("Invalid response value. Response:\n" << _response);
      _response = "";
      return false;
    }
  } else {
      LOG_ERROR(" Invalid response - no param or fault tag. Response:\n" << _response);
    _response = "";
    return false;
  }

  _response = "";
  return result.valid();
}


// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
