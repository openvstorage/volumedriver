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

#ifndef BACKEND_CONNECTION_POOL_H_
#define BACKEND_CONNECTION_POOL_H_

#include "BackendConnectionInterface.h"
#include "ConnectionDeleter.h"

#include <iosfwd>
#include <memory>

#include <boost/chrono.hpp>
#include <boost/intrusive/slist.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/EnableMakeShared.h>
#include <youtils/Logging.h>
#include <youtils/SpinLock.h>

VD_BOOLEAN_ENUM(ForceNewConnection)

namespace backend
{

class BackendConfig;
class BackendInterface;
class Namespace;

using BackendConnectionInterfacePtr = std::unique_ptr<BackendConnectionInterface,
                                                      ConnectionDeleter>;

class ConnectionPool
    : public std::enable_shared_from_this<ConnectionPool>
{
public:
    static std::shared_ptr<ConnectionPool>
    create(std::unique_ptr<BackendConfig>,
           size_t capacity);

    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&) = delete;

    ConnectionPool&
    operator=(const ConnectionPool&) = delete;

    BackendConnectionInterfacePtr
    get_connection(const ForceNewConnection = ForceNewConnection::F);

    void
    capacity(const size_t);

    size_t
    capacity() const;

    size_t
    size() const;

    const BackendConfig&
    config() const
    {
        return *config_;
    }

    struct Counters
    {
        uint64_t gets_new = 0;
        uint64_t gets_from_pool = 0;
        uint64_t puts_delete = 0;
        uint64_t puts_to_pool = 0;
    };

    Counters
    counters() const;

    using Clock = boost::chrono::steady_clock;

    void
    error();

    Clock::time_point
    last_error() const;

private:
    DECLARE_LOGGER("BackendConnectionPool");

    friend class youtils::EnableMakeShared<ConnectionPool>;
    friend class ConnectionDeleter;

    ConnectionPool(std::unique_ptr<BackendConfig>,
                   size_t);

    mutable fungi::SpinLock lock_;

    using Connections = boost::intrusive::slist<BackendConnectionInterface>;
    Connections connections_;

    std::unique_ptr<BackendConfig> config_;
    size_t capacity_;
    Counters counters_;
    Clock::time_point last_error_;

    std::unique_ptr<BackendConnectionInterface>
    make_one_();

    static std::unique_ptr<BackendConnectionInterface>
    pop_(Connections&);

    static void
    clear_(Connections&);

    void
    release_connection_(BackendConnectionInterface*);
};

std::ostream&
operator<<(std::ostream&,
           const ConnectionPool::Counters&);

}

#endif // !BACKEND_CONNECTION_POOL_H_
