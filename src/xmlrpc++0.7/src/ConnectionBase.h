#ifndef _CONNECTION_BASE_H_
#define _CONNECTION_BASE_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//

#include <string>

#include <youtils/Logging.h>

#include "XmlRpcValue.h"
#include "XmlRpcSource.h"
#include "ServerBase.h"

namespace xmlrpc
{
namespace x = ::XmlRpc;

// The server waits for client connections and provides methods
class XmlRpcServerMethod;

  //! A class to handle XML RPC requests from a particular client
  class ConnectionBase
  {
      static const char METHODNAME_TAG[];
      static const char PARAMS_TAG[];
      static const char PARAMS_ETAG[];
      static const char PARAM_TAG[];
      static const char PARAM_ETAG[];

      static const std::string SYSTEM_MULTICALL;
      static const std::string METHODNAME;
      static const std::string PARAMS;

      static const std::string FAULTCODE;
      static const std::string FAULTSTRING;

  public:
      //! Constructor
      ConnectionBase(ServerBase* _server);
      //! Destructor
      virtual ~ConnectionBase();

    //   // XmlRpcSource interface implementation
    // //! Handle IO on the client connection socket.
    // //!   @param eventType Type of IO event that occurred. @see XmlRpcDispatch::EventType.
    // virtual unsigned handleEvent(unsigned eventType);

      DECLARE_LOGGER("ConnectionBase");

  protected:

      bool
      readHeader(int fd,
                 bool& keepAlive);

      bool
      readRequest(int fd);

      bool
      writeResponse(int fd,
                    bool retval);

      // Parses the request, runs the method, generates the response xml.
      virtual void executeRequest();

      // Parse the methodName and parameters from the request.
      std::string parseRequest(x::XmlRpcValue& params);

      // Execute a named method with the specified params.
      bool executeMethod(const std::string& methodName,
                         x::XmlRpcValue& params,
                         x::XmlRpcValue& result);

      // Execute multiple calls and return the results in an array.
      bool executeMulticall(const std::string& methodName,
                            x::XmlRpcValue& params,
                            x::XmlRpcValue& result);

      // Construct a response from the result XML.
      void generateResponse(std::string const& resultXml);
      void generateFaultResponse(std::string const& msg,
                                 int errorCode = -1);
      std::string generateHeader(std::string const& body);


      // The XmlRpc server that accepted this connection
      ::xmlrpc::ServerBase* _server;

      // Possible IO states for the connection
      enum ServerConnectionState { READ_HEADER, READ_REQUEST, WRITE_RESPONSE };
      ServerConnectionState _connectionState;

      // Request headers
      std::string _header;

      // Number of bytes expected in the request body (parsed from header)
      int _contentLength;

      // Request body
      std::string _request;

      // Response
      std::string _response;

      // Number of bytes of the response written so far
      int _bytesWritten;

      // // Whether to keep the current client connection open for further requests
      // bool _keepAlive;
  };
}

#endif // _XMLRPCSERVERCONNECTION_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
