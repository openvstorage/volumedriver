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

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <string>

namespace youtils
{

class SocketAddress
{
public:
    SocketAddress();
    SocketAddress(const struct sockaddr& s);
    SocketAddress(const struct sockaddr_in& s);
    SocketAddress(const struct sockaddr_in6& s);
    SocketAddress(const struct sockaddr_storage& s);
    SocketAddress(const SocketAddress& other);
    ~SocketAddress();

    const SocketAddress&
    operator=(const SocketAddress& other);

    const SocketAddress&
    operator=(const struct sockaddr& s);

    const SocketAddress&
    operator=(const struct sockaddr_in& s);

    const SocketAddress&
    operator=(const struct sockaddr_in6& s);

    const SocketAddress&
    operator=(const struct sockaddr_storage& s);

    static socklen_t
    get_family_length(sa_family_t family)
    {
        switch (family)
        {
        case AF_INET:
            return sizeof(struct sockaddr_in);
        case AF_INET6:
            return sizeof(struct sockaddr_in6);
        }
        //cnanakos: assert?
        return 0;
    }

    sa_family_t
    get_family() const;

    socklen_t
    get_length() const;

    std::string
    get_ip_address() const;

    uint16_t
    get_port() const;

    struct sockaddr&
    sockaddr()
    {
        return socket_addr_.sa;
    }

    const struct sockaddr&
    sockaddr() const
    {
        return socket_addr_.sa;
    }

    struct sockaddr_in&
    sockaddr_in()
    {
        return socket_addr_.sa_ipv4;
    }

    const struct sockaddr_in&
    sockaddr_in() const
    {
        return socket_addr_.sa_ipv4;
    }

    struct sockaddr_in6&
    sockaddr_in6()
    {
        return socket_addr_.sa_ipv6;
    }

    const struct sockaddr_in6&
    sockaddr_in6() const
    {
        return socket_addr_.sa_ipv6;
    }

    struct sockaddr_storage&
    sockaddr_storage()
    {
        return socket_addr_.sa_storage;
    }

    const struct sockaddr_storage&
    sockaddr_storage() const
    {
        return socket_addr_.sa_storage;
    }

    operator struct sockaddr* ()
    {
        return &socket_addr_.sa;
    }

    operator const struct sockaddr* () const
    {
        return &socket_addr_.sa;
    }

    operator struct sockaddr_in* ()
    {
        return &socket_addr_.sa_ipv4;
    }

    operator const struct sockaddr_in* () const
    {
        return &socket_addr_.sa_ipv4;
    }

    operator struct sockaddr_in6* ()
    {
        return &socket_addr_.sa_ipv6;
    }

    operator const struct sockaddr_in6* () const
    {
        return &socket_addr_.sa_ipv6;
    }

    operator struct sockaddr_storage* ()
    {
        return &socket_addr_.sa_storage;
    }

    operator const struct sockaddr_storage* () const
    {
        return &socket_addr_.sa_storage;
    }

private:
    typedef union sockaddr_t_
    {
        struct sockaddr         sa;
        struct sockaddr_in      sa_ipv4;
        struct sockaddr_in6     sa_ipv6;
        struct sockaddr_storage sa_storage;
    } sockaddr_t;
    sockaddr_t socket_addr_;
};

} //namespace youtils
