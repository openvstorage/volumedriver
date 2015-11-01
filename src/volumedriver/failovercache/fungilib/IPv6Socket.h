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
