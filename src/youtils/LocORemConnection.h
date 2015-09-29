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

#ifndef YOUTILS_LOCOREM_CONNECTION_H_
#define YOUTILS_LOCOREM_CONNECTION_H_

#include "Assert.h"
#include "Catchers.h"
#include "IOException.h"
#include "Logging.h"

#include <chrono>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>

namespace youtils
{

template<typename S>
class LocORemConnection
    : public std::enable_shared_from_this<LocORemConnection<S>>
{
public:
    using Type = LocORemConnection<S>;
    using Ptr = std::shared_ptr<Type>;

    MAKE_EXCEPTION(Exception, fungi::IOException);
    MAKE_EXCEPTION(ShortReadException, Exception);
    MAKE_EXCEPTION(ShortWriteException, Exception);

    ~LocORemConnection()
    {
        LOG_INFO(this << ": terminating");
    }

    LocORemConnection(const LocORemConnection&) = delete;

    LocORemConnection&
    operator=(const LocORemConnection&) = delete;

    static Ptr
    create(S sock)
    {
        auto conn(Ptr(new LocORemConnection<S>(std::move(sock))));
        conn->init_();
        return conn;
    }

    // TODO: unify async_{read,write}
    template<typename BufferSequence,
             typename Fun>
    void
    async_read(const BufferSequence& bufs,
               Fun&& fun,
               const boost::optional<std::chrono::seconds>& timeout = boost::none)
    {
        const bool use_timer = timeout != boost::none;
        const std::size_t bytes_expected(boost::asio::buffer_size(bufs));
        auto self(this->shared_from_this());

        // LOG_TRACE(this << ": scheduling read of " << bytes_expected << " bytes");

        auto f([bytes_expected,
                fun = std::move(fun),
                self,
                this,
                use_timer](const boost::system::error_code& ec,
                           std::size_t bytes_transferred) mutable
               {
                   // LOG_TRACE(this << ": read completion: " << ec.message() <<
                   //           ", expected " << bytes_expected <<
                   //           ", transferred " << bytes_transferred);

                   disable_deadline_();

                   if (ec)
                   {
                       LOG_ERROR(this << ": async read returned error " <<
                                 ec.message());
                       throw boost::system::system_error(ec);
                   }

                   // TODO: we might want to simply pass `bytes_transferred' to `fun'?
                   if (bytes_expected != bytes_transferred)
                   {
                       LOG_ERROR(this << ": read less (" << bytes_transferred <<
                                 ") than expected (" << bytes_expected << ")");
                       throw ShortReadException("read less than expected");
                   }

                   fun(*self);
               });

        boost::asio::async_read(sock_,
                                bufs,
                                strand_.wrap(std::move(f)));

        if (use_timer)
        {
            deadline_.expires_from_now(*timeout);
            check_deadline_();
        }
    }

    template<typename BufferSequence,
             typename Fun>
    void
    async_write(const BufferSequence& bufs,
                Fun&& fun,
                const boost::optional<std::chrono::seconds>& timeout = boost::none)
    {
        const bool use_timer = timeout != boost::none;
        const std::size_t bytes_expected(boost::asio::buffer_size(bufs));
        auto self(this->shared_from_this());

        // LOG_TRACE(this << ": scheduling write of " << bytes_expected << " bytes");

        auto f([bytes_expected,
                fun = std::move(fun),
                self,
                this,
                use_timer](const boost::system::error_code& ec,
                           std::size_t bytes_transferred) mutable
               {
                   // LOG_TRACE(this << ": write completion: " << ec.message() <<
                   //           " - expected " << bytes_expected << ", sent " <<
                   //           bytes_transferred);

                   disable_deadline_();

                   if (ec)
                   {
                       LOG_ERROR(this << ": async write returned error " <<
                                 ec.message());
                       throw boost::system::system_error(ec);
                   }

                   // TODO: we might want to simply pass `bytes_transferred' to `fun'?
                   if (bytes_expected != bytes_transferred)
                   {
                       LOG_ERROR(this << ": wrote less (" << bytes_transferred <<
                                 ") than expected (" << bytes_expected << ")");
                       throw ShortWriteException("wrote less than expected");
                   }

                   fun(*self);
               });

        boost::asio::async_write(sock_,
                                 bufs,
                                 strand_.wrap(std::move(f)));

        if (use_timer)
        {
            deadline_.expires_from_now(*timeout);
            check_deadline_();
        }
    }

    template<typename Fun>
    void
    async_work(Fun&& fun,
               const boost::optional<std::chrono::seconds>& timeout = boost::none)
    {
        const bool use_timer = timeout != boost::none;
        auto self(this->shared_from_this());

        // LOG_TRACE(this << ": scheduling work");

        auto f([fun = std::move(fun),
                self,
                this,
                use_timer]() mutable
               {
                   // LOG_TRACE(this << ": work completion");
                   disable_deadline_();

                   fun(*self);
               });

        strand_.get_io_service().post(strand_.wrap(std::move(f)));

        if (use_timer)
        {
            deadline_.expires_from_now(*timeout);
            check_deadline_();
        }
    }

private:
    DECLARE_LOGGER("LocORemConnection");

    S sock_;
    boost::asio::strand strand_;
    boost::asio::steady_timer deadline_;

    explicit LocORemConnection(S sock)
        : sock_(std::move(sock))
        , strand_(sock_.get_io_service())
        , deadline_(sock_.get_io_service())
    {
        LOG_INFO(this << ": new connection " << sock_.local_endpoint() <<
                 " (local) <-> " << sock_.remote_endpoint() << " (remote)");
    }

    // Moved out of the constructor as the caller needs to hold the instance in a
    // shared_ptr b/c check_deadline_() uses shared_from_this()
    void
    init_()
    {
        disable_deadline_();
        check_deadline_();
    }

    void
    disable_deadline_()
    {
        deadline_.expires_at(std::chrono::steady_clock::time_point::max());
    }

    void
    check_deadline_()
    {
        if (deadline_.expires_at() <= std::chrono::steady_clock::now())
        {
            LOG_ERROR(this << ": timeout, closing socket");

            boost::system::error_code ec;
            sock_.close(ec);

            if (ec)
            {
                LOG_ERROR(this << ": failed to close socket: " <<
                          ec.message() << " - ignoring");
            }
        }
        else
        {
            auto self(this->shared_from_this());

            auto f([self, this](const boost::system::error_code& ec)
                   {
                       if (ec == boost::system::errc::operation_canceled)
                       {
                           // LOG_TRACE(this << ": timer cancelled");
                       }
                       else
                       {
                           if (ec)
                           {
                               LOG_ERROR(this << ": timer reported error " <<
                                         ec.message() << " - closing socket");
                               sock_.close();
                           }
                           else
                           {
                               LOG_INFO(this << ": timer fired");
                           }

                           check_deadline_();
                       }
                   });

            deadline_.async_wait(strand_.wrap(f));
        }
    }
};

using UnixSocket = boost::asio::local::stream_protocol::socket;
using TcpSocket = boost::asio::ip::tcp::socket;

using LocORemUnixConnection = LocORemConnection<UnixSocket>;
using LocORemTcpConnection = LocORemConnection<TcpSocket>;

}

#endif // !YOUTILS_LOCOREM_CONNECTION_H_
