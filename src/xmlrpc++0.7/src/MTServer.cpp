#include "XmlRpcSocket.h"

#include "MTServer.h"
#include "MTConnection.h"
#include <youtils/Assert.h>

namespace xmlrpc
{


#define LOCK_SERVER                             \
    lock_type::scoped_lock l__(lock_)

#define ASSERT_LOCKED
const boost::posix_time::time_duration
MTServer::join_timeout = boost::posix_time::milliseconds(0);

namespace x = XmlRpc;

MTServer::MTServer()
    : fd_(-1)
    , port_(0)
    , accepting_connections_(true)
{
    LOG_INFO("MT SERVER");
}


MTServer::~MTServer()
{
    LOCK_SERVER;
    shutdown();
}

void
MTServer::stopAcceptingConnections()
{
    LOCK_SERVER;
    accepting_connections_ = false;
}

void
MTServer::startAcceptingConnections()
{
    LOCK_SERVER;
    accepting_connections_ = true;
}

void
MTServer::acceptConnection()
{
    {
        LOCK_SERVER;
        if(not accepting_connections_)
        {
            int sock = x::XmlRpcSocket::accept(fd_);
            if(sock >= 0)
            {
                x::XmlRpcSocket::close(sock);
            }
        }

    }

    int sock = x::XmlRpcSocket::accept(fd_);
    if (sock < 0)
    {
        LOG_ERROR("Failed to accept connection: " << x::XmlRpcSocket::getErrorMsg());
    }
    else if (not x::XmlRpcSocket::setNonBlocking(sock))
    {
        LOG_ERROR("Failed to set socket non-blocking: " << x::XmlRpcSocket::getErrorMsg());
        x::XmlRpcSocket::close(sock);
    }
    else
    {
        LOCK_SERVER;
        LOG_DEBUG("Creating new connection");
        try
        {
            MTConnection connection(sock, this);

            //            conns_.insert(new XMLRPCConnection(sock, this));
            boost::thread* t = new boost::thread(connection);
            LOCK_SERVER;
            threads_.push_back(t);
        }
        catch (std::exception &e)
        {
            LOG_ERROR("Failed to create new connection: " << e.what());
        }
        catch (...)
        {
            LOG_ERROR("Failed to create new connection: unknown exception");
        }
    }
}

bool
MTServer::bindAndListen(const boost::optional<std::string>& addr,
                        int port,
                        int backlog /*= 5*/)
{
    LOG_INFO("Port " << port << " Backlog " << backlog);

    fd_ = XmlRpcSocket::socket();
    if (fd_ < 0)
    {
        LOG_ERROR("Could not create socket, "
                  << XmlRpcSocket::getErrorMsg());
        return false;
    }

    //    this->setfd(fd);

    //   // Don't block on reads/writes
    if (! XmlRpcSocket::setNonBlocking(fd_))
    {
        close();
        LOG_ERROR("Could not set socket to non-blocking input mode " <<
                  XmlRpcSocket::getErrorMsg());
        return false;
    }

  // Allow this port to be re-bound immediately so server re-starts are not delayed
    if ( ! XmlRpcSocket::setReuseAddr(fd_))
  {
      close();
      LOG_ERROR("Could not set SO_REUSEADDR socket option "
                << XmlRpcSocket::getErrorMsg());
      return false;
  }

    // Bind to the specified port on the default interface
    if ( ! XmlRpcSocket::bind(addr,
                              fd_,
                              port))
    {
        close();
        LOG_ERROR("Could not bind to specified port, "
                  << XmlRpcSocket::getErrorMsg());
        return false;
    }

    // Set in listening mode
    if ( ! XmlRpcSocket::listen(fd_, backlog))
    {
        close();
        LOG_ERROR("Could not set socket in listening mode, "
                  << XmlRpcSocket::getErrorMsg());
        return false;
    }
    port_ = port;
    LOG_INFO("MTServer listening on port " << port << ", fd " << fd_);

    // Notify the dispatcher to listen on this source when we are in work()
    //  _disp.addSource(this, XmlRpcDispatch::ReadableEvent);

    poll_fd_.fd = fd_;
    poll_fd_.events = POLLIN | POLLERR;
    poll_fd_.revents = 0;

    return true;
}

void
MTServer::close()
{
    if(fd_ >= 0)
    {
        LOG_INFO("Closing fd " << fd_);
        XmlRpcSocket::close(fd_);
        fd_ = -1;
    }
    else
    {
        LOG_INFO("Not closing fd " << fd_);
    }
}



uint64_t
MTServer::cleanup_threads_locked()
{
    LOG_INFO("Trying to join " << threads_.size() << " threads");
    LOCK_SERVER;
    std::list<boost::thread*>::iterator it = threads_.begin();
    while(it != threads_.end())
    {

        if((*it)->timed_join(join_timeout))
        {
            delete *it;
            it = threads_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    return threads_.size();
}


uint64_t
MTServer::num_connections()
{
    return cleanup_threads_locked();
}


// Process client requests for the specified time
void
MTServer::work(double timeout)
{
    //    LOG_INFO("Polling for a connection");

    while(true)
    {
       int nEvents = poll(&poll_fd_, 1, timeout * 1000);


        if(nEvents == -1 and errno == EINTR)
        {
            continue;
        }
        else if (nEvents < 0)
        {
            LOG_INFO("Error in poll " << strerror(errno));
            close();
            return;
        }
        else if (nEvents == 0)
        {
            return;
        }
        else
        {
            acceptConnection();
        }
        {
            LOG_INFO("You have " << threads_.size() << " connections running ");
            cleanup_threads_locked();

        }
    }
}

// Stop processing client requests
void
MTServer::exit()
{
    LOG_INFO("Exiting the work procedure ");

}

// Close the server socket file descriptor and stop monitoring connections
void
MTServer::shutdown()
{
  // This closes and destroys all connections as well as closing this socket
    LOG_INFO("Shutting down the server");

    close();
    {
        LOCK_SERVER;
        cleanup_threads_locked();

    }
    if(threads_.size() == 0)
    {
        return;
    }
    {
        LOG_INFO("First stage: still " << threads_.size() << " Threads not joinable");
        cleanup_threads_locked();

    }
    if(threads_.size() == 0)
    {
        return;
    }
    LOG_INFO("Still " << threads_.size() << " Threads not joinable");

    {
        LOCK_SERVER;
        std::list<boost::thread*>::iterator it = threads_.begin();
        const boost::posix_time::time_duration  new_timeout = boost::posix_time::milliseconds(10000);

        while(it != threads_.end())
        {
            if((*it)->timed_join(new_timeout))
            {
                delete *it;
                it = threads_.erase(it);
            }
            else
            {
                (*it)->detach();
                delete *it;
                it = threads_.erase(it);
            }
        }
    }
}

}
// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
