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

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <sstream>
#include <msgpack.hpp>

#include "TCPServer.h"

using boost::asio::ip::tcp;

namespace ganesha
{
ovs_discovery_server::ovs_discovery_server(const int& port,
                                           const size_t& thread_pool_size,
                                           FileSystemWrapper *fsw)
    : io_service_()
    , acceptor_(io_service_, tcp::endpoint(tcp::v4(), port))
    , thread_pool_size_(thread_pool_size)
    , fsw_(fsw)
    , grp_(NULL)
{
    if (thread_pool_size == 0)
    {
        //try continue running with only one thread of execution
        thread_pool_size_ = 1;
    }
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    grp_ = new boost::thread_group();
    start_accept();
}

void
ovs_discovery_server::run()
{
    for (size_t i = 0; i < thread_pool_size_; i++)
    {
        boost::thread *thread(new boost::thread(
                    boost::bind(&boost::asio::io_service::run, &io_service_)));
        grp_->add_thread(thread);
    }
}

void
ovs_discovery_server::stop()
{
    io_service_.stop();
    grp_->join_all();
    delete grp_;
}

void
ovs_discovery_server::handle_write()
{
}

std::string
ovs_discovery_server::ovs_create_msg()
{
    std::stringstream buffer;
    msgpack::pack(buffer, fsw_->ovs_discovery_message());
    return buffer.str();
}

void
ovs_discovery_server::handle_accept(const boost::system::error_code& ec,
                                    boost::shared_ptr<tcp::socket> socket)
{
    if (!ec || ec == boost::asio::error::message_size)
    {

        boost::asio::async_write(*socket, boost::asio::buffer(ovs_create_msg()),
                                 boost::bind(&ovs_discovery_server::handle_write,
                                             this));
    }
    start_accept();
}

void
ovs_discovery_server::start_accept()
{
    boost::shared_ptr<tcp::socket> socket(new tcp::socket(io_service_));
    acceptor_.async_accept(*socket,
            boost::bind(&ovs_discovery_server::handle_accept,
                        this,
                        boost::asio::placeholders::error,
                        socket));
}

}
