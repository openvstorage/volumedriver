#ifndef XMLRPC_SERVER_H_
#define XMLRPC_SERVER_H_

#include "XmlRpc.h"
#include "XmlRpcServerConnection.h"

#include <poll.h>

#include <list>
#include <set>

#include <boost/thread.hpp>
#include <boost/utility.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <youtils/Logging.h>


namespace xmlrpc
{

using namespace XmlRpc;

class MTServer
    : public ServerBase
{
public:
    MTServer();

    MTServer(const MTServer&) = delete;

    MTServer&
    operator=(const MTServer&) = delete;

    virtual ~MTServer();

    virtual void
    acceptConnection() override;

    virtual bool
    bindAndListen(const boost::optional<std::string>& addr,
                  int port,
                  int backlog /*= 5*/) override;

    virtual void
    work(double timeout) override;

    virtual void
    exit() override;

    virtual void
    shutdown() override;

    virtual uint64_t
    num_connections() override;

    virtual void
    stopAcceptingConnections() override;

    virtual void
    startAcceptingConnections() override;

private:
    DECLARE_LOGGER("MTServer");

    uint64_t
    cleanup_threads_locked();

    // this needs to be recursive, as removeConnection is called
    // from the Connection's destructor.
    typedef boost::recursive_mutex lock_type;
    mutable lock_type lock_;

    int fd_;

    void
    close();

    uint16_t port_;

    struct pollfd poll_fd_;

    static const boost::posix_time::time_duration join_timeout;

    std::list<boost::thread*> threads_;

    bool accepting_connections_;

};

}

#endif // !XMLRPC_SERVER_H_

// Local Variables: **
// mode: c++ **
// End: **
