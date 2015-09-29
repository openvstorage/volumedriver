#include "MTConnection.h"
#include "MTServer.h"
#include "XmlRpcSocket.h"

namespace xmlrpc
{
namespace x = ::XmlRpc;

MTConnection::MTConnection(int sock,
                           MTServer* server)
    : ConnectionBase(server)
{
    LOG_TRACE("Constructor");
    poll_fd_.fd = sock;
    poll_fd_.events = POLLIN | POLLOUT | POLLERR;
    poll_fd_.revents = 0;
}

MTConnection::~MTConnection()
{
    LOG_TRACE("Destructor");
}

void
MTConnection::operator()()
{
    try
    {
        while(true)
        {
            bool keepAlive = false;
            int nEvents = poll(&poll_fd_, 1, -1);
            if(nEvents == -1 and errno == EINTR)
            {
                continue;
            }
            else if (nEvents < 0)
            {
                LOG_INFO("Error in poll " << strerror(errno));
                x::XmlRpcSocket::close(poll_fd_.fd);
                return;
            }

            if (_connectionState == READ_HEADER)
            {
                if (not readHeader(poll_fd_.fd,
                                   keepAlive))
                {
                    x::XmlRpcSocket::close(poll_fd_.fd);
                    return;
                }
            }

            if (_connectionState == READ_REQUEST)
            {
                if (! readRequest(poll_fd_.fd))
                {
                    x::XmlRpcSocket::close(poll_fd_.fd);
                    return;
                }
            }

            if (_connectionState == WRITE_RESPONSE)
            {

                if (! writeResponse(poll_fd_.fd,
                                    false))
                {

                    LOG_INFO("Closing socket " << poll_fd_.fd);
                    x::XmlRpcSocket::close(poll_fd_.fd);
                    return;
                }
            }
        }
    }
    catch(std::exception& e)
    {
        LOG_INFO("Error handling the connection: " << e.what() << ", closing socket " << poll_fd_.fd);
        x::XmlRpcSocket::close(poll_fd_.fd);
    }
    catch(...)
    {
        LOG_INFO("Unknown error handling the connection, closing socket " << poll_fd_.fd);
        x::XmlRpcSocket::close(poll_fd_.fd);
    }

}

}

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
