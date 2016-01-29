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

#ifndef YT_REDIS_QUEUE_H_
#define YT_REDIS_QUEUE_H_

#include "RedisUrl.h"
#include "IOException.h"

#include <memory>
#include <hiredis/hiredis.h>
#include <iostream>

namespace youtils
{

MAKE_EXCEPTION(RedisQueueException, fungi::IOException);
MAKE_EXCEPTION(RedisQueuePushException, RedisQueueException);
MAKE_EXCEPTION(RedisQueuePopException, RedisQueueException);
MAKE_EXCEPTION(RedisQueueConnectionException, RedisQueueException);

template<typename T>
struct RedisValueTraits
{};

template<>
struct RedisValueTraits<std::string>
{
    static const char*
    to_bytes(const std::string& str)
    {
        return str.c_str();
    }

    static std::string
    from_bytes(const char* buf)
    {
        if (buf)
        {
            return std::string(buf);
        }
        else
        {
            return std::string();
        }
    }
};

class RedisReply
{
public:
    explicit RedisReply(void* r)
    : reply_(reinterpret_cast<redisReply*>(r))
    {};

    ~RedisReply()
    {
        if (reply_)
        {
            freeReplyObject(reply_);
        }
    }

    int
    type()
    {
        return reply_->type;
    }

    const char*
    str()
    {
        return reply_->str;
    }

private:
    redisReply* reply_;
};

class RedisQueue
{
public:
    explicit RedisQueue(const RedisUrl& url,
                        const int timeout)
    : _context(nullptr)
    , _host(url.host)
    , _port(url.port)
    , _key(url.key)
    , _timeout(timeout)
    {
        connect();
    }

    ~RedisQueue()
    {
        if (_context)
        {
            ::redisFree(_context);
        }
    }

    template<typename T, typename Traits = RedisValueTraits<T>>
    void
    push(const T& t)
    {
        RedisReply reply(::redisCommand(_context,
                                        "LPUSH %s %s",
                                        key(),
                                        Traits::to_bytes(t)));
        if (reply.type() == REDIS_REPLY_ERROR)
        {
            throw RedisQueuePushException("cannot push value, err: ",
                                          reply.str());
        }
    }

    template<typename T, typename Traits = RedisValueTraits<T>>
    T
    pop()
    {
        RedisReply reply(::redisCommand(_context,
                                        "LPOP %s",
                                        key()));
        switch (reply.type())
        {
        case REDIS_REPLY_ERROR:
            throw RedisQueuePopException("cannot pop value, err: ",
                                         reply.str());
        case REDIS_REPLY_STRING:
            return Traits::from_bytes(reply.str());
        default:
            return T();
        }
    }

    const char* host() const
    {
        return _host.c_str();
    }

    const char* key() const
    {
        return _key.c_str();
    }

    int port() const
    {
        return _port;
    }

private:
    void connect() const
    {
        if (not _context)
        {
            struct timeval to = {_timeout, 0};
            _context = ::redisConnectWithTimeout(host(),
                                                 port(),
                                                 to);
            if (_context == nullptr || _context->err)
            {
                if (_context)
                {
                    RedisQueueConnectionException r("connection error: ",
                                                    _context->errstr);
                    ::redisFree(_context);
                    throw r;
                }
                else
                {
                    throw RedisQueueConnectionException("cannot allocate "
                                                        "redis context");
                }
            }
        }
    }

private:
    mutable redisContext* _context;
    std::string _host;
    uint16_t _port;
    std::string _key;
    int _timeout;
};

} //namespace youtils

#endif //YT_REDIS_QUEUE_H_
