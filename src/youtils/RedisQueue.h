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

struct RedisReplyDeleter
{

    void
    operator()(redisReply* r)
    {
        if (r)
        {
            freeReplyObject(r);
        }
    }
};

using RedisReplyPtr = std::unique_ptr<redisReply, RedisReplyDeleter>;

class RedisQueue
{
public:
    explicit RedisQueue(const RedisUrl& url,
                        const int timeout)
    : _context(nullptr)
    , _host(url.host())
    , _port(url.port() ? *url.port() : 0)
    , _key(url.path())
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
        const RedisReplyPtr reply(
                static_cast<redisReply*>(::redisCommand(_context,
                                                        "RPUSH %s %s",
                                                        key(),
                                                        Traits::to_bytes(t))));
        if (reply)
        {
            if (reply->type == REDIS_REPLY_ERROR)
            {
                throw RedisQueuePushException("cannot push value, err: ",
                                              reply->str);
            }
        }
        else
        {
            throw RedisQueuePushException("cannot push value, reply is empty");
        }
    }

    template<typename T, typename Traits = RedisValueTraits<T>>
    T
    pop()
    {
        const RedisReplyPtr reply(
            static_cast<redisReply*>(::redisCommand(_context,
                                                    "LPOP %s",
                                                    key())));
        if (reply)
        {
            switch (reply->type)
            {
            case REDIS_REPLY_ERROR:
                throw RedisQueuePopException("cannot pop value, err: ",
                                             reply->str);
            case REDIS_REPLY_STRING:
                return Traits::from_bytes(reply->str);
            case REDIS_REPLY_NIL:
                return T();
            default:
                throw RedisQueuePopException("unsupported type: ",
                                             std::to_string(reply->type).c_str());
            }
        }
        else
        {
            throw RedisQueuePopException("cannot pop value, reply is empty");
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

using RedisQueuePtr = std::unique_ptr<RedisQueue>;

} //namespace youtils

#endif //YT_REDIS_QUEUE_H_
