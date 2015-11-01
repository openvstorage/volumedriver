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
