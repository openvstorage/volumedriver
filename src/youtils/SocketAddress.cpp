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

#include "SocketAddress.h"

#include <arpa/inet.h>
#include <string.h>

namespace youtils
{

SocketAddress::SocketAddress()
{
    memset(&socket_addr_, 0, sizeof(socket_addr_));
}

SocketAddress::SocketAddress(const struct sockaddr& s)
{
    socket_addr_.sa = s;
}

SocketAddress::SocketAddress(const struct sockaddr_in& s)
{
    socket_addr_.sa_ipv4 = s;
}

SocketAddress::SocketAddress(const struct sockaddr_in6& s)
{
    socket_addr_.sa_ipv6 = s;
}

SocketAddress::SocketAddress(const struct sockaddr_storage& s)
{
    socket_addr_.sa_storage = s;
}

SocketAddress::SocketAddress(const SocketAddress& other)
    : socket_addr_(other.socket_addr_)
{
}

SocketAddress::~SocketAddress()
{
}

sa_family_t
SocketAddress::get_family() const
{
    return socket_addr_.sa.sa_family;
}

socklen_t
SocketAddress::get_length() const
{
    return get_family_length(get_family());
}

std::string
SocketAddress::get_ip_address() const
{
    char str[INET6_ADDRSTRLEN] = {0};
    switch (get_family())
    {
    case AF_INET:
        if (inet_ntop(AF_INET,
                      &socket_addr_.sa_ipv4.sin_addr,
                      str,
                      sizeof(str)))
        {
            return str;
        }
        break;
    case AF_INET6:
        if (inet_ntop(AF_INET6,
                      &socket_addr_.sa_ipv6.sin6_addr,
                      str,
                      sizeof(str)))
        {
            return str;
        }
        break;
    }
    return "";
}

uint16_t
SocketAddress::get_port() const
{
    switch (get_family())
    {
    case AF_INET:
        return ntohs(socket_addr_.sa_ipv4.sin_port);
    case AF_INET6:
        return ntohs(socket_addr_.sa_ipv6.sin6_port);
    }
    return 0;
}

} //namespace youtils
