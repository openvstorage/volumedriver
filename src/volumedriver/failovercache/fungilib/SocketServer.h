// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

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
