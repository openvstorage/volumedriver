#ifndef XMLRPC_CONNECTION_H_
#define XMLRPC_CONNECTION_H_

#include <memory>

#include <boost/utility.hpp>

#include <poll.h>
#include "XmlRpc.h"
#include "XmlRpcServerConnection.h"
#include "XmlRpcDispatch.h"

#include <youtils/Logging.h>

namespace xmlrpc
{

class MTServer;

class MTConnection
    : public ConnectionBase
{

    friend class MTServer;

public:
    virtual ~MTConnection();


    void
    operator()();

private:
    DECLARE_LOGGER("XMLRPCConnection");


    struct pollfd poll_fd_;


    MTConnection(int sock,
                 MTServer*);
};

}

#endif // !XMLRPC_CONNECTION_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: *
