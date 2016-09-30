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

#ifndef IPV4SOCKET_H_
#define IPV4SOCKET_H_

#include "defines.h"
#include "Socket.h"


namespace fungi
{

class IPv4Socket
    : public Socket
{
public:
    explicit IPv4Socket(bool rdma, int sock_type = SOCK_STREAM);

    virtual ~IPv4Socket() {}

    IPv4Socket(const IPv4Socket&) = delete;

    IPv4Socket&
    operator=(const IPv4Socket&) = delete;

    virtual const std::string
    toString() const override;

    virtual void
    bind(const boost::optional<std::string>& addr,
         uint16_t port,
         bool reuseaddr = true) override;

    virtual Socket
    *accept(bool nonblocking = true) override;

    virtual bool
    connect_nb(const std::string &host, uint16_t port) override;

private:
    DECLARE_LOGGER("IPv4Socket");

    IPv4Socket(const Socket *parent_socket,
               int asock,
               int sock_type_,
               const char *remote_ip,
               uint16_t remote_port);

    int
    poll_(pollfd*,
          nfds_t,
          const boost::optional<boost::chrono::milliseconds>&) override;
};

}

#endif /* IPV4SOCKET_H_ */
