// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef YT_REDIS_URL_H_
#define YT_REDIS_URL_H_

#include <iosfwd>
#include <string>

namespace youtils
{

struct RedisUrl
{
    static const uint16_t default_port;

    std::string host;
    uint16_t port = default_port;
    std::string key;

    explicit RedisUrl(std::string h,
                      uint16_t p = default_port,
                      std::string k = std::string("/"))
        : host(std::move(h))
        , port(p)
        , key(std::move(k))
    {}

    RedisUrl() = default;

    ~RedisUrl() = default;

    RedisUrl(const RedisUrl&) = default;

    RedisUrl&
    operator=(const RedisUrl&) = default;

    RedisUrl(RedisUrl&&) = default;

    RedisUrl&
    operator=(RedisUrl&&) = default;

    bool
    operator==(const RedisUrl& other) const
    {
        return
            host == other.host and
            port == other.port and
            key == other.key;
    }

    bool
    operator!=(const RedisUrl& other) const
    {
        return not operator==(other);
    }

    static bool
    is_one(const std::string&);
};

std::ostream&
operator<<(std::ostream&,
           const RedisUrl&);

std::istream&
operator>>(std::istream&,
           RedisUrl&);

} //namespace youtils


#endif //YT_REDIS_URL_H_
