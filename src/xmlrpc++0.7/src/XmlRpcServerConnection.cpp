
#include "XmlRpcServerConnection.h"
#include "XmlRpcServer.h"
#include "XmlRpcSocket.h"
#include "XmlRpc.h"
# include <stdio.h>
# include <stdlib.h>
#include <string.h>
using namespace XmlRpc;

// Static data
// const char XmlRpcServerConnection::METHODNAME_TAG[] = "<methodName>";
// const char XmlRpcServerConnection::PARAMS_TAG[] = "<params>";
// const char XmlRpcServerConnection::PARAMS_ETAG[] = "</params>";
// const char XmlRpcServerConnection::PARAM_TAG[] = "<param>";
// const char XmlRpcServerConnection::PARAM_ETAG[] = "</param>";

// const std::string XmlRpcServerConnection::SYSTEM_MULTICALL = "system.multicall";
// const std::string XmlRpcServerConnection::METHODNAME = "methodName";
// const std::string XmlRpcServerConnection::PARAMS = "params";

// const std::string XmlRpcServerConnection::FAULTCODE = "faultCode";
// const std::string XmlRpcServerConnection::FAULTSTRING = "faultString";



// The server delegates handling client requests to a serverConnection object.
XmlRpcServerConnection::XmlRpcServerConnection(int fd,
                                               xmlrpc::XmlRpcServer* server,
                                               bool deleteOnClose /*= false*/)
    : XmlRpcSource(fd, deleteOnClose)
    , ConnectionBase(server)
{
    LOG_DEBUG(" new socket" << fd);
  //  _server = server;
  //  _connectionState = READ_HEADER;
  _keepAlive = true;
}


XmlRpcServerConnection::~XmlRpcServerConnection()
{
    LOG_DEBUG("Destructor");
    static_cast<xmlrpc::XmlRpcServer*>(_server)->removeConnection(this);
}


// Handle input on the server socket by accepting the connection
// and reading the rpc request. Return true to continue to monitor
// the socket for events, false to remove it from the dispatcher.
unsigned
XmlRpcServerConnection::handleEvent(unsigned /*eventType*/)
{
    if (_connectionState == READ_HEADER)
    {

        if (! readHeader(getfd(),
                         _keepAlive))
        {
            return 0;
        }
    }


    if (_connectionState == READ_REQUEST)
    {
        if (! readRequest(getfd()))
        {
            return 0;
        }
    }

    if (_connectionState == WRITE_RESPONSE)
    {

        if ( ! writeResponse(getfd(),
                             _keepAlive))
        {
            return 0;
        }
    }


    return (_connectionState == WRITE_RESPONSE)
        ? XmlRpcDispatch::WritableEvent :
        XmlRpcDispatch::ReadableEvent;
}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
