// Copyright 2015 Open vStorage NV
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

#include "Assert.h"
#include "Catchers.h"
#include "LocORemServer.h"

#include <boost/asio/error.hpp>
#include <boost/lexical_cast.hpp>

namespace youtils
{

namespace ba = boost::asio;
namespace bs = boost::system;

LocORemServer::~LocORemServer()
{
    try
    {
        stop_();
    }
    CATCH_STD_ALL_LOG_IGNORE(port_ << ": failed to stop worker threads");
}

void
LocORemServer::stop_()
{
    LOG_INFO(port_ << ": stopping");

    bs::error_code ec;

    io_service_.stop();

    unix_acceptor_.close(ec);
    if (ec)
    {
        LOG_ERROR(port_ << "failed to close unix acceptor: " << ec.message() <<
                  " (ignoring)");
    }

    tcp_acceptor_.close(ec);
    if (ec)
    {
        LOG_ERROR(port_ << ": failed to close TCP acceptor: " << ec.message() <<
                  " (ignoring)");
    }

    threads_.join_all();

    LOG_INFO(port_ << ": done");
}

LocORemLocalAddress
LocORemServer::local_address(const uint16_t port)
{
    static const char buf[] = "\0ovs.locorem:";

    return LocORemLocalAddress(std::string(buf, sizeof(buf)) +
                               boost::lexical_cast<std::string>(port));
}

LocORemLocalAddress
LocORemServer::local_address() const
{
    return local_address(port_);
}

void
LocORemServer::work_()
{
    while (true)
    {
        try
        {
            io_service_.run();
            LOG_INFO("I/O service exited, breaking out of loop");
            return;
        }
        catch (bs::system_error& e)
        {
            const bs::error_code& ec = e.code();

            if (ec.category() == ba::error::misc_category and
                ec.value() == ba::error::misc_errors::eof)
            {
                LOG_INFO(ec.message() << ": connection closed");
            }
            else if (ec.category() == ba::error::system_category)
            {
                switch (ec.value())
                {
                case bs::errc::operation_canceled:
                    {
                        LOG_INFO("Shutdown requested, breaking out of loop");
                        return;
                    }
                case bs::errc::broken_pipe:
                case bs::errc::connection_aborted:
                case bs::errc::connection_reset:
                    {
                        LOG_INFO(ec.message() << " - proceeding");
                        break;
                    }
                default:
                    {
                        LOG_ERROR(ec.message() << " - proceeding anyway");
                        break;
                    }
                }
            }
            else
            {
                LOG_ERROR(ec.message() << " - proceeding anyway");
            }
        }
        CATCH_STD_ALL_LOG_IGNORE("Caught exception in server thread");
    }
}

}
