#include "XmlRpcServer.h"
#include "XmlRpcServerConnection.h"
#include "XmlRpcSocket.h"
#include "XmlRpcUtil.h"
namespace xmlrpc
{
using namespace XmlRpc;

// Accept a client connection request and create a connection to
// handle method calls from the client.
XmlRpcServer::XmlRpcServer()
{}

XmlRpcServer::~XmlRpcServer()
{
    shutdown();
}

void
XmlRpcServer::acceptConnection()
{
    int s = XmlRpcSocket::accept(this->getfd());
    LOG_DEBUG("socket " << s);
    if (s < 0)
    {
        close();
        LOG_ERROR("ServerBase::acceptConnection: Could not accept connection, "
                  << XmlRpcSocket::getErrorMsg());
    }
    else if ( ! XmlRpcSocket::setNonBlocking(s))
    {
        XmlRpcSocket::close(s);
        LOG_ERROR("ServerBase::acceptConnection: Could not set socket to non-blocking input mode, "
                          << XmlRpcSocket::getErrorMsg());
    }
    else  // Notify the dispatcher to listen for input on this source when we are in work()
    {
        LOG_DEBUG("creating a connection");
        _disp.addSource(this->createConnection(s), XmlRpcDispatch::ReadableEvent);
    }
}

void
XmlRpcServer::removeConnection(XmlRpcServerConnection* sc)
{
    _disp.removeSource(sc);
}

unsigned
XmlRpcServer::handleEvent(unsigned /*mask*/)
{
  acceptConnection();
  return XmlRpcDispatch::ReadableEvent;		// Continue to monitor this fd
}

// Create a new connection object for processing requests from a specific client.
XmlRpcServerConnection*
XmlRpcServer::createConnection(int s)
{
  // Specify that the connection object be deleted when it is closed
  return new XmlRpcServerConnection(s, this, true);
}

// Create a socket, bind to the specified port, and
// set it in listen mode to make it available for clients.
bool
XmlRpcServer::bindAndListen(const boost::optional<std::string>& addr,
                            int port,
                            int backlog /*= 5*/)
{
  int fd = XmlRpcSocket::socket();
  if (fd < 0)
  {
      LOG_ERROR("Could not create socket " <<  XmlRpcSocket::getErrorMsg());
    return false;
  }

  this->setfd(fd);

  // Don't block on reads/writes
  if ( ! XmlRpcSocket::setNonBlocking(fd))
  {
    this->close();
    LOG_ERROR("Could not set socket to non-blocking input mode " <<  XmlRpcSocket::getErrorMsg());
    return false;
  }

  // Allow this port to be re-bound immediately so server re-starts are not delayed
  if ( ! XmlRpcSocket::setReuseAddr(fd))
  {
    this->close();
    LOG_ERROR("Could not set SO_REUSEADDR socket option " <<  XmlRpcSocket::getErrorMsg());
    return false;
  }

  // Bind to the specified port on the default interface
  if ( !XmlRpcSocket::bind(addr, fd, port) )
  {
    this->close();
    LOG_ERROR("Could not bind to specified port (%s)." << XmlRpcSocket::getErrorMsg());
    return false;
  }

  // Set in listening mode
  if ( ! XmlRpcSocket::listen(fd, backlog))
  {
    this->close();
    LOG_ERROR("Could not set socket in listening mode " << XmlRpcSocket::getErrorMsg());
    return false;
  }

  LOG_DEBUG("server listening on port" << port << " fd " <<  fd);

  // Notify the dispatcher to listen on this source when we are in work()
  _disp.addSource(this, XmlRpcDispatch::ReadableEvent);

  return true;
}



// Process client requests for the specified time
void
XmlRpcServer::work(double timeout)
{
    LOG_DEBUG("Waiting for a connection");
  _disp.work(timeout);
}

// Stop processing client requests
void
XmlRpcServer::exit()
{
  _disp.exit();
}

// Close the server socket file descriptor and stop monitoring connections
void
XmlRpcServer::shutdown()
{
  // This closes and destroys all connections as well as closing this socket
  _disp.clear();
}


}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
