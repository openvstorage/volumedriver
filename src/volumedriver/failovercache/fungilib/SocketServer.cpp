// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "IPv4Socket.h"
#include "IPv6Socket.h"
#include "Protocol.h"
#include "ProtocolFactory.h"
#include "SocketServer.h"
#include "Thread.h"

#include <cstdlib>
#include <signal.h>
#include <sys/socket.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#include <rdma/rdma_cma.h>
#include <rdma/rsocket.h>

#include <boost/optional/optional_io.hpp>

#include <youtils/Catchers.h>
#include <youtils/IOException.h>

#include "use_rs.h"

//#undef LOG_DEBUG
//#define LOG_DEBUG LOG_INFO

namespace fungi
{

using namespace std::string_literals;

SocketServer::SocketServer(ProtocolFactory& factory,
                           const boost::optional<std::string>& addr,
                           uint16_t port,
                           bool rdma)
    : factory_(factory)
    , addr_(addr)
    , port_(port)
    , server_socket_(Socket::createSocket(rdma))
    , thread_(nullptr)
    , name_("SocketServer_"s + factory.getName())
    , use_rs_(server_socket_->isRdma())
{
    server_socket_->setName(name_);
    if(pipe(pipes_) != 0)
    {
        throw fungi::IOException("could not not open pipe");
    }
}

SocketServer::~SocketServer()
{
    server_socket_.reset();
    close(pipes_[0]);
    close(pipes_[1]);
}

std::unique_ptr<SocketServer>
SocketServer::createSocketServer(ProtocolFactory& factory,
                                 const boost::optional<std::string>& addr,
                                 uint16_t port,
                                 bool rdma)
{
    std::unique_ptr<SocketServer> ss(new SocketServer(factory,
                                                      addr,
                                                      port,
                                                      rdma));
    ss->start();
    return ss;
}

void SocketServer::start()
{
    server_socket_->bind(addr_,
                         port_);
    server_socket_->listen();
    LOG_DEBUG("SocketServer("<<name_.c_str()<<")::listening on " <<
              addr_ << ":" << port_ );
    // make listen socket nonblocking to allow
    // stopping gracefully via non-blocking accept
    // this is only still needed on win32 for now, because
    // they don't support SIGUSR1
#ifdef _WIN32
    server_socket_->setNonBlocking();
#else
    // strictly speaking not needed but we do it anyway, because
    // OS behavior is not always logical
    server_socket_->clearNonBlocking();
#endif
    // update: no longer do nonblocking for accept()
    // reason: we want our daemon to be completely idle
    // when not doing anything, instead of polling
    // around accept()
    thread_ = ThreadPtr(new Thread(*this),
                        ThreadDeleter());

    thread_->start();
}

void SocketServer::run()
{
    LOG_DEBUG("SocketServer("<<name_.c_str()<<")::run");

    while (true)
    {
        try
        {
            struct pollfd fds[2];
            fds[0].fd = server_socket_->fileno();
            fds[1].fd = pipes_[0];

            fds[0].events = POLLIN | POLLPRI;
            fds[1].events = POLLIN | POLLPRI;
            fds[0].revents = 0;
            fds[1].revents = 0;

            int res = rs_poll(fds, 2, -1);

            LOG_DEBUG("rs_poll returned rc=" << res << " errno=" << errno << " revents=[" << fds[0].revents << "],[" << fds[1].revents << "]");

            if(res < 0)
            {
                if(errno == EINTR)
                {
                    continue;
                }
                else
                {
                    LOG_ERROR("Error in select");
                    throw fungi::IOException("Error in select");
                }
            }
            else if(fds[1].revents)
            {
                LOG_DEBUG("SocketServer(" << name_ <<
                          "): interrupted accept and I want to stop");
                break;
            }
            else
            {
                Socket *asock = server_socket_->accept();
                Protocol* protocol = factory_.createProtocol(asock,
                                                             *this);
                protocol->start();
            }
        }
        CATCH_STD_ALL_LOGLEVEL_RETHROW("accept interupted", DEBUG);
    }
}

void SocketServer::stop()
{
    LOG_DEBUG("Stop requested")
    ssize_t ret;
    ret = write(pipes_[1],"a",1);
    if(ret < 0) {
        LOG_ERROR("Failed to send stop request to thread: " << strerror(errno));
    }
}

const char *SocketServer::getName() const
{
    return name_.c_str();
}

void SocketServer::join()
{
    thread_->join();
}

}
