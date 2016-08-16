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

#ifndef YOUTILS_LOCOREM_CLIENT_H_
#define YOUTILS_LOCOREM_CLIENT_H_

#include "BooleanEnum.h"
#include "LocORemConnection.h"
#include "Logging.h"

#include <chrono>

#include <boost/asio.hpp>
#include <boost/variant.hpp>

VD_BOOLEAN_ENUM(ForceRemote);

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
