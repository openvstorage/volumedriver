// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef YOUTILS_LOCOREM_CLIENT_H_
#define YOUTILS_LOCOREM_CLIENT_H_

#include "BooleanEnum.h"
#include "LocORemConnection.h"
#include "Logging.h"

#include <chrono>

#include <boost/asio.hpp>
#include <boost/variant.hpp>

BOOLEAN_ENUM(ForceRemote);

namespace youtils
{

class LocORemClient
{
public:
    LocORemClient(const std::string& addr,
                  uint16_t port,
                  const boost::optional<std::chrono::seconds>& connect_timeout = boost::none,
                  ForceRemote force_remote = ForceRemote::F);

    ~LocORemClient() = default;

    LocORemClient(const LocORemClient&) = delete;

    LocORemClient&
    operator=(const LocORemClient&) = delete;

    template<typename BufferSequence>
    void
    send(const BufferSequence& bufs,
         const boost::optional<std::chrono::seconds>& timeout)
    {
        SendVisitor<BufferSequence> v(bufs,
                                      timeout,
                                      io_service_);
        boost::apply_visitor(v,
                             conn_);
    }

    template<typename BufferSequence>
    void
    recv(const BufferSequence& bufs,
         const boost::optional<std::chrono::seconds>& timeout)
    {
        RecvVisitor<BufferSequence> v(bufs,
                                      timeout,
                                      io_service_);
        boost::apply_visitor(v,
                             conn_);
    }

    bool
    is_local() const;

private:
    DECLARE_LOGGER("LocORemClient");

    boost::asio::io_service io_service_;

    boost::variant<LocORemUnixConnection::Ptr,
                   LocORemTcpConnection::Ptr> conn_;

    // TODO: reduce code duplication between Send- and RecvVisitor
    template<typename BufferSequence>
    struct RecvVisitor
        : public boost::static_visitor<>
    {
        const BufferSequence& bufs_;
        const boost::optional<std::chrono::seconds>& timeout_;
        boost::asio::io_service& io_service_;

        RecvVisitor(const BufferSequence& bufs,
                    const boost::optional<std::chrono::seconds>& timeout,
                    boost::asio::io_service& io_service)
            : bufs_(bufs)
            , timeout_(timeout)
            , io_service_(io_service)
        {}

        template<typename C>
        void
        operator()(C conn)
        {
            // LOG_TRACE("receiving");
            io_service_.reset();

            auto fun([&](typename C::element_type&)
                     {
                         // LOG_TRACE("done receiving");
                     });

            conn->async_read(bufs_,
                             std::move(fun),
                             timeout_);

            io_service_.run();
            // LOG_TRACE("io service is done");
        }
    };

    template<typename BufferSequence>
    struct SendVisitor
        : public boost::static_visitor<>
    {
        const BufferSequence& bufs_;
        const boost::optional<std::chrono::seconds>& timeout_;
        boost::asio::io_service& io_service_;

        SendVisitor(const BufferSequence& bufs,
                    const boost::optional<std::chrono::seconds>& timeout,
                    boost::asio::io_service& io_service)
            : bufs_(bufs)
            , timeout_(timeout)
            , io_service_(io_service)
        {}

        template<typename C>
        void
        operator()(C conn)
        {
            // LOG_TRACE("sending");

            io_service_.reset();

            auto fun([&](typename C::element_type&)
                     {
                         // LOG_TRACE("done sending");
                     });

            conn->async_write(bufs_,
                              std::move(fun),
                              timeout_);

            io_service_.run();
            // LOG_TRACE("io service is done");
        }
    };
};

}

#endif // !YOUTILS_LOCOREM_CLIENT_H_
