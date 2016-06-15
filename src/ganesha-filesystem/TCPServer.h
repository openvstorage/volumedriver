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

#ifndef __TCP_SERVER_H
#define __TCP_SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>

#include "FileSystemWrapper.h"

namespace ganesha
{

class ovs_discovery_server:
    private boost::noncopyable
{
public:
    explicit ovs_discovery_server(const int& port,
                                  const size_t& thread_pool_size,
                                  FileSystemWrapper *fsw);

    void
    run();

    void
    stop();

private:
    void
    handle_write();

    void
    handle_accept(const boost::system::error_code& ec,
                  boost::shared_ptr<boost::asio::ip::tcp::socket> socket);

    void
    start_accept();

    std::string
    ovs_create_msg();

    boost::asio::io_service io_service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    size_t thread_pool_size_;
    FileSystemWrapper *fsw_;
    boost::thread_group *grp_;
};

}

#endif
