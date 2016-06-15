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

#ifndef IPV6SOCKET_H_
#define IPV6SOCKET_H_

#include "Socket.h"
#include <youtils/Logging.h>

namespace fungi
{

class IPv6Socket
    : public Socket
{
public:
    explicit IPv6Socket(int sock_type = SOCK_STREAM);

    ~IPv6Socket() = default;

    IPv6Socket(const IPv6Socket&) = delete;

    IPv6Socket&
    operator=(const IPv6Socket&) = delete;

    virtual const std::string
    toString() const override;

    virtual void
    bind(const boost::optional<std::string>& addr,
         uint16_t port,
         bool reuseaddr = true) override;

    virtual Socket*
    accept(bool nonblocking = true) override;

    virtual void
    connect(const std::string &host, uint16_t port) override;

    virtual bool
    connect_nb(const std::string &host, uint16_t port) override;

private:
    DECLARE_LOGGER("IPv6Socket");

    IPv6Socket(const Socket *parent_socket,
               int asock,
               int sock_type_,
               const char *str,
               uint16_t remote_port);
};

}

#endif /* IPV6SOCKET_H_ */
