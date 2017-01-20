#include "MTConnection.h"
#include "MTServer.h"
#include "XmlRpcSocket.h"

namespace xmlrpc
{

namespace x = ::XmlRpc;

namespace
{

const std::string thread_pfx("xmlrpc_conn/");

}

MTConnection::MTConnection(int sock,
                           MTServer* server)
    : ConnectionBase(server)
{
    poll_fd_.fd = sock;
    poll_fd_.events = POLLIN | POLLOUT | POLLERR;
    poll_fd_.revents = 0;
}

void
MTConnection::operator()()
{
    const std::string name(thread_pfx + std::to_string(poll_fd_.fd));
    pthread_setname_np(pthread_self(),
                       name.c_str());
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
// mode: c++ **
// End: **
