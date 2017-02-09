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

#include "AlbaConfig.h"
#include "Alba_Connection.h"
#include "BackendConfig.h"
#include "ConnectionPool.h"
#include "LocalConfig.h"
#include "Local_Connection.h"
#include "S3Config.h"
#include "S3_Connection.h"

#include <iostream>

#include <boost/thread/lock_guard.hpp>

#include <youtils/Catchers.h>

namespace backend
{

#define LOCK()                                                          \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

namespace yt = youtils;

ConnectionPool::ConnectionPool(std::unique_ptr<BackendConfig> config,
                               size_t capacity)
    : config_(std::move(config))
    , capacity_(capacity)
    , last_error_(Clock::time_point::min())
{
    VERIFY(config_->backend_type.value() != BackendType::MULTI);
    LOG_INFO("Created pool for " << *config_ << ", capacity " << capacity);
}

ConnectionPool::~ConnectionPool()
{
    clear_(connections_);
}

std::shared_ptr<ConnectionPool>
ConnectionPool::create(std::unique_ptr<BackendConfig> config,
                       size_t capacity)
{
    return std::make_shared<yt::EnableMakeShared<ConnectionPool>>(std::move(config),
                                                                  capacity);
}

std::unique_ptr<BackendConnectionInterface>
ConnectionPool::pop_(Connections& conns)
{
    std::unique_ptr<BackendConnectionInterface> c;
    if (not conns.empty())
    {
        c = std::unique_ptr<BackendConnectionInterface>(&conns.front());
        conns.pop_front();
    }

    return c;
}

void
ConnectionPool::clear_(Connections& conns)
{
    while (not conns.empty())
    {
        pop_(conns);
    }
}

std::unique_ptr<BackendConnectionInterface>
ConnectionPool::make_one_()
try
{
    switch (config_->backend_type.value())
    {
    case BackendType::LOCAL:
        {
            const LocalConfig* config(dynamic_cast<const LocalConfig*>(config_.get()));
            return std::make_unique<local::Connection>(*config);
        }
    case BackendType::S3:
        {
            const S3Config* config(dynamic_cast<const S3Config*>(config_.get()));
            return std::make_unique<s3::Connection>(*config);
        }
    case BackendType::ALBA:
        {
            const AlbaConfig* config(dynamic_cast<const AlbaConfig*>(config_.get()));
            return std::make_unique<albaconn::Connection>(*config);
        }
    case BackendType::MULTI:
        {
            VERIFY(0 == "how did a MULTI config end up here!?");
        }
    }
    UNREACHABLE
}
CATCH_STD_ALL_EWHAT({
        LOG_ERROR(*config_ << ": failed to create new connection:"  << EWHAT);
        error_();
        throw;
    })

void
ConnectionPool::release_connection_(BackendConnectionInterface* conn)
{
    {
        LOCK();

        if (conn)
        {
            if (conn->healthy() and connections_.size() < capacity_)
            {
                ++counters_.puts_to_pool;
                connections_.push_front(*conn);
                return;
            }
            else
            {
                ++counters_.puts_delete;
            }
        }
    }

    delete conn;
}

BackendConnectionInterfacePtr
ConnectionPool::get_connection(ForceNewConnection force_new)
{
    ConnectionDeleter d(shared_from_this());
    BackendConnectionInterfacePtr conn(nullptr, d);

    if (force_new == ForceNewConnection::F)
    {
        LOCK();
        ++counters_.gets_from_pool;
        conn = BackendConnectionInterfacePtr(pop_(connections_).release(), d);
    }

    if (not conn)
    {
        conn = BackendConnectionInterfacePtr(make_one_().release(), d);
        LOCK();
        ++counters_.gets_new;
    }

    ASSERT(conn);
    return conn;
}

size_t
ConnectionPool::size() const
{
    LOCK();
    return connections_.size();
}

size_t
ConnectionPool::capacity() const
{
    LOCK();
    return capacity_;
}

void
ConnectionPool::capacity(size_t cap)
{
    Connections tmp;

    {
        LOCK();

        std::swap(capacity_, cap);
        if (connections_.size() > capacity_)
        {
            for (size_t i = 0; i < capacity_; ++i)
            {
                BackendConnectionInterface& c = connections_.front();
                connections_.pop_front();
                tmp.push_front(c);
            }

            std::swap(tmp, connections_);
        }
    }

    clear_(tmp);

    LOG_INFO(*config_ << ": updated capacity from " <<
             cap << " to " << capacity());
}

ConnectionPool::Counters
ConnectionPool::counters() const
{
    LOCK();
    return counters_;
}

std::ostream&
operator<<(std::ostream& os,
           const ConnectionPool::Counters& c)
{
    return os <<
        "gets_new=" << c.gets_new <<
        ",gets_from_pool=" << c.gets_from_pool <<
        ",puts_delete=" << c.puts_delete <<
        ",puts_to_pool=" << c.puts_to_pool;
}

ConnectionPool::Clock::time_point
ConnectionPool::last_error() const
{
    LOCK();
    return last_error_;
}

void
ConnectionPool::error_()
{
    LOCK();
    last_error_ = Clock::now();
}

}
