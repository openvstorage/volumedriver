// Copyright 2016 iNuron NV
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

#ifndef YT_ETCD_URL_H_
#define YT_ETCD_URL_H_

#include <iosfwd>
#include <string>

namespace youtils
{

struct EtcdUrl
{
    static const uint16_t default_port;

    std::string host;
    uint16_t port = default_port;
    std::string key;


    explicit EtcdUrl(std::string h,
                     uint16_t p = default_port,
                     std::string k = std::string("/"))
        : host(std::move(h))
        , port(p)
        , key(std::move(k))
    {}

    EtcdUrl() = default;

    ~EtcdUrl() = default;

    EtcdUrl(const EtcdUrl&) = default;

    EtcdUrl&
    operator=(const EtcdUrl&) = default;

    EtcdUrl(EtcdUrl&&) = default;

    EtcdUrl&
    operator=(EtcdUrl&&) = default;

    bool
    operator==(const EtcdUrl& other) const
    {
        return
            host == other.host and
            port == other.port and
            key == other.key;
    }

    bool
    operator!=(const EtcdUrl& other) const
    {
        return not operator==(other);
    }

    static bool
    is_one(const std::string&);
};

std::ostream&
operator<<(std::ostream&,
           const EtcdUrl&);

std::istream&
operator>>(std::istream&,
           EtcdUrl&);

}

#endif // YT_ETCD_URL_H_
