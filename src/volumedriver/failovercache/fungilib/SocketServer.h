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

#ifndef _SOCKETSERVER_H
#define	_SOCKETSERVER_H

#include "defines.h"
#include "Runnable.h"
#include "Thread.h"

#include <atomic>
#include <memory>
#include <string>

#include <boost/optional.hpp>

#include <youtils/Logging.h>

namespace fungi
{

class ProtocolFactory;
class Socket;

class SocketServer
    : private Runnable
{
public:
    static std::unique_ptr<SocketServer>
    createSocketServer(ProtocolFactory& factory,
                       const boost::optional<std::string>& addr,
                       uint16_t port,
                       bool rdma);

    virtual ~SocketServer();

    SocketServer(const SocketServer&) = delete;

    SocketServer&
    operator=(const SocketServer&) = delete;

    virtual void
    run() override final;

    virtual const char*
    getName() const override final;

    void
    join();

    void
    stop();

private:
    DECLARE_LOGGER("SocketServer");

    SocketServer();

    SocketServer(ProtocolFactory& factory,
                 const boost::optional<std::string>& addr,
                 uint16_t port,
                 bool rdma);

    void
    start();

    ProtocolFactory& factory_;
    const boost::optional<std::string> addr_;
    const int port_;
    std::unique_ptr<Socket> server_socket_;

    // The creators of fungi::Thread thought it wise to do a delete *this in ->destroy().
    // Oh well. Kill it, quick!
    struct ThreadDeleter
    {
        void
        operator()(Thread* t)
        {
            if (t != nullptr)
            {
                t->destroy();
            }
        }
    };

    using ThreadPtr = std::unique_ptr<Thread, ThreadDeleter>;
    ThreadPtr thread_;

    const std::string name_;

    bool use_rs_;
    int pipes_[2];
};

}

#endif	/* _SOCKETSERVER_H */
