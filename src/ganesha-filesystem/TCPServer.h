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
