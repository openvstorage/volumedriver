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
#include "LocORemClient.h"
#include "LocORemConnection.h"
#include "LocORemServer.h"

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <boost/scope_exit.hpp>

namespace youtils
{

namespace ba = boost::asio;
namespace bs = boost::system;

namespace
{

DECLARE_LOGGER("LocORemClientHelpers");

TODO("AR: add support for IP address resolution");
TODO("AR: see if there's something less lowlevel available eg. in boost");

bool
is_local(const std::string& addr)
{
    LOG_TRACE("Checking whether " << addr << " is local");

    ifaddrs* addrs = nullptr;

    int ret = getifaddrs(&addrs);
    if (ret != 0)
    {
        LOG_ERROR("Failed to get interface addresses: " << strerror(errno) <<
                  " - assuming " << addr << " is not local");
        return false;
    }

    BOOST_SCOPE_EXIT((&addrs))
    {
        freeifaddrs(addrs);
    }
    BOOST_SCOPE_EXIT_END;

    std::vector<char> buf(INET6_ADDRSTRLEN);

    for (const ifaddrs* ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
            continue;

        const void* in_addr = nullptr;

        switch (ifa->ifa_addr->sa_family)
        {
        case AF_INET:
            {
                const sockaddr_in* s4 =
                    reinterpret_cast<const sockaddr_in*>(ifa->ifa_addr);
                in_addr = &s4->sin_addr;
                break;
            }
        case AF_INET6:
            {
                const sockaddr_in6 *s6 =
                    reinterpret_cast<const sockaddr_in6*>(ifa->ifa_addr);
                in_addr = &s6->sin6_addr;
                break;
            }
        default:
            continue;
        }

        auto p = inet_ntop(ifa->ifa_addr->sa_family,
                           in_addr,
                           buf.data(),
                           buf.size());
        if (p == nullptr)
        {
            LOG_ERROR("Failed to get IP address of " << ifa->ifa_name << ": " <<
                      strerror(errno) << " - ignoring");
        }

        const std::string s(buf.data());

        LOG_TRACE(ifa->ifa_name << " has IP " << s);
        if (s == addr)
        {
            LOG_INFO(addr << " is a local address");
            return true;
        }
    }

    LOG_INFO(addr << " does not appear to be a local address");
    return false;
}

template<typename S>
auto
do_connect(ba::io_service& io_service,
           const typename S::endpoint_type& ep,
           const boost::optional<std::chrono::seconds>& connect_timeout)
{
    io_service.reset();

    S sock(io_service);
    ba::strand strand(io_service);
    bool connected = false;

    boost::asio::steady_timer deadline(io_service);

    if (connect_timeout)
    {
        auto f([&](const bs::error_code& ec)
               {
                   if (ec == boost::system::errc::operation_canceled)
                   {
                       LOG_TRACE(ep << ": timer cancelled");
                       VERIFY(connected);
                   }
                   else if (ec)
                   {
                       LOG_ERROR(ep << ": timer aborted with error: " << ec.message());
                   }
                   else
                   {
                       LOG_ERROR(ep << ": timeout trying to connect");
                   }

                   if (not connected)
                   {
                       LOG_ERROR(ep << ": we're not connected yet");
                       bs::error_code e;
                       sock.close(e);
                       if (e)
                       {
                           LOG_ERROR(ep << ": failed to close socket: " <<
                                     e.message() << " - ignoring");
                       }

                       throw bs::system_error(ec ?
                                              ec :
                                              bs::error_code(ETIME,
                                                             bs::system_category()));
                   }
               });

        deadline.expires_from_now(*connect_timeout);
        deadline.async_wait(strand.wrap(f));
    }

    auto g([&](const bs::error_code& ec)
           {
               if (ec)
               {
                   LOG_ERROR(ep << ": failed to connect: " << ec.message());
                   throw bs::system_error(ec);
               }

               connected = true;
               if (connect_timeout)
               {
                   deadline.cancel();
               }

               LOG_INFO(ep << ": connected");
           });

    sock.async_connect(ep,
                       strand.wrap(g));

    io_service.run();

    VERIFY(connected);

    return LocORemConnection<S>::create(std::move(sock));
}

boost::variant<LocORemUnixConnection::Ptr,
               LocORemTcpConnection::Ptr>
connect(ba::io_service& io_service,
        const std::string& addr,
        uint16_t port,
        const boost::optional<std::chrono::seconds>& connect_timeout,
        ForceRemote force_remote)
{
    bool remote = force_remote == ForceRemote::T;
    if (remote)
    {
        LOG_INFO("reluctantly attempting remote connection per the caller's request");
    }
    else
    {
        remote = not is_local(addr);
    }

    if (remote)
    {
        const TcpSocket::endpoint_type ep(ba::ip::address::from_string(addr),
                                          port);

        return do_connect<TcpSocket>(io_service,
                                     ep,
                                     connect_timeout);
    }
    else
    {
        const UnixSocket::endpoint_type
            ep(LocORemServer::local_address(port).str());

        return do_connect<UnixSocket>(io_service,
                                      ep,
                                      connect_timeout);
    }
}

}

LocORemClient::LocORemClient(const std::string& addr,
                             uint16_t port,
                             const boost::optional<std::chrono::seconds>& connect_timeout,
                             ForceRemote force_remote)
    : conn_(connect(io_service_,
                    addr,
                    port,
                    connect_timeout,
                    force_remote))
{}

bool
LocORemClient::is_local() const
{
    struct V
        : public boost::static_visitor<>
    {
        bool local = false;

        void
        operator()(LocORemUnixConnection::Ptr)
        {
            local = true;
        }

        void
        operator()(LocORemTcpConnection::Ptr)
        {
            local = false;
        }
    };

    V v;

    boost::apply_visitor(v,
                         conn_);
    return v.local;
}

}
