
#ifndef _XMLRPCSERVER_H_
#define _XMLRPCSERVER_H_
//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//

#include <youtils/Logging.h>

#include "ServerBase.h"

namespace XmlRpc
{
// Class representing connections to specific clients
class XmlRpcServerConnection;

}

namespace xmlrpc {
using namespace XmlRpc;

class XmlRpcServer : public ServerBase , public XmlRpcSource
{
public:
    XmlRpcServer();

    virtual ~XmlRpcServer();

    void
    removeConnection(XmlRpcServerConnection*);

private:
    DECLARE_LOGGER("XmlRpcServer");

    //! Accept a client connection request
    virtual void
    acceptConnection() override;

    //! Remove a connection from the dispatcher

    virtual bool
    bindAndListen(const boost::optional<std::string>& addr,
                  int port,
                  int backlog /*= 5*/) override;

    virtual void
    work(double timeout) override;


    virtual void
    exit() override;

    //! Handle client connection requests
    virtual unsigned
    handleEvent(unsigned eventType) override;

    virtual void
    shutdown() override;

    //! Create a new connection object for processing requests from a specific client.
    XmlRpcServerConnection* createConnection(int socket);

    // Event dispatcher
    XmlRpcDispatch _disp;

    virtual uint64_t
    num_connections() override
    {
        return 1;
    }
};

} // namespace XmlRpc

#endif //_XMLRPCSERVER_H_

// Local Variables: **
// mode: c++ **
// End: **
