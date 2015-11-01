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

#ifndef YOUTILS_LOCOREM_SERVER_H_
#define YOUTILS_LOCOREM_SERVER_H_

#include "LocORemConnection.h"
#include "Logging.h"
#include "StrongTypedString.h"

#include <boost/asio.hpp>
#include <boost/thread.hpp>

// We use the abstract namespace (linux only!) instead of the portable file representation
// to avoid cluttering up the filesystem and having to deal with crash recovery measures.
// That means that the first byte is \0.
STRONG_TYPED_STRING(youtils, LocORemLocalAddress);

namespace youtils
{

class LocORemServer
{
public:
    template<typename UnixFun,
             typename TcpFun>
    LocORemServer(const std::string& addr,
                  const uint16_t port,
                  UnixFun&& unix_fun,
                  TcpFun&& tcp_fun,
                  size_t nthreads = boost::thread::hardware_concurrency())
        : unix_sock_(io_service_)
        , tcp_sock_(io_service_)
        , unix_strand_(io_service_)
        , tcp_strand_(io_service_)
        , unix_acceptor_(io_service_,
                         boost::asio::local::stream_protocol::endpoint(local_address(port).str()))
        , tcp_acceptor_(io_service_,
                        boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(addr),
                                                       port))
        , port_(port)
    {
        VERIFY(nthreads > 0);

        accept_(unix_acceptor_,
                unix_strand_,
                unix_sock_,
                std::move(unix_fun));

        accept_(tcp_acceptor_,
                tcp_strand_,
                tcp_sock_,
                std::move(tcp_fun));

        try
        {
            for (size_t i = 0; i < nthreads; ++i)
            {
                threads_.create_thread(boost::bind(&LocORemServer::work_,
                                                   this));
            }
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to create worker pool: " << EWHAT);
                stop_();
                throw;
            });

        LOG_INFO("Listening on TCP port " << port_ << " / Unix port " <<
                 local_address().str().substr(1) << " (abstract NS), using " <<
                 threads_.size() << " threads");
    }

    ~LocORemServer();

    LocORemServer(const LocORemServer&) = delete;

    LocORemServer&
    operator=(const LocORemServer&) = delete;

    static LocORemLocalAddress
    local_address(const uint16_t port);

    LocORemLocalAddress
    local_address() const;

private:
    DECLARE_LOGGER("LocORemServer");

    boost::asio::io_service io_service_;
    UnixSocket unix_sock_;
    TcpSocket tcp_sock_;

    boost::asio::strand unix_strand_;
    boost::asio::strand tcp_strand_;

    boost::asio::local::stream_protocol::acceptor unix_acceptor_;
    boost::asio::ip::tcp::acceptor tcp_acceptor_;

    boost::thread_group threads_;

    const uint16_t port_;

    void
    stop_();

    template<typename Acceptor,
             typename Strand,
             typename Sock,
             typename Callback>
    void
    accept_(Acceptor& acceptor,
            Strand& strand,
            Sock& sock,
            Callback&& callback)
    {
        auto fun([&,
                  callback = std::move(callback)]
                 (const boost::system::error_code& ec) mutable
                 {
                     LOG_TRACE(port_ << ": " << sock.local_endpoint() <<
                               " (local) <-> " << sock.remote_endpoint() <<
                               " (remote), " << ec.message());

                     using namespace boost::system::errc;

                     switch (ec.value())
                     {
                     case errc_t::success:
                         {
                             auto c(LocORemConnection<Sock>::create(std::move(sock)));
                             callback(*c);
                             break;
                         }
                     case errc_t::connection_aborted:
                         {
                             LOG_TRACE(port_ << ": remote gave up");
                             break;
                         }
                     default:
                         {
                             LOG_ERROR(port_ <<
                                       ": error during accept, bailing out: " <<
                                       ec.message());
                             return;
                         }
                     }

                     sock = Sock(io_service_);
                     accept_(acceptor,
                             strand,
                             sock,
                             std::move(callback));
                 });

        acceptor.async_accept(sock,
                              strand.wrap(std::move(fun)));

    }

    void
    work_();
};

}

#endif // !YOUTILS_LOCOREM_SERVER_H_
