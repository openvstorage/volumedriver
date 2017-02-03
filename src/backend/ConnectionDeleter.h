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

#ifndef BACKEND_CONNECTION_DELETER_H_
#define BACKEND_CONNECTION_DELETER_H_

#include <memory>

namespace backend
{

class BackendConnectionInterface;
class ConnectionPool;

class ConnectionDeleter
{
public:
    explicit ConnectionDeleter(const std::shared_ptr<ConnectionPool>& pool)
        : pool_(pool)
    {}

    ~ConnectionDeleter() = default;

    ConnectionDeleter(const ConnectionDeleter&) = default;

    ConnectionDeleter(ConnectionDeleter&&) = default;

    ConnectionDeleter&
    operator=(const ConnectionDeleter&) = default;

    ConnectionDeleter&
    operator=(ConnectionDeleter&&) = default;

    void
    operator()(BackendConnectionInterface*);

    std::shared_ptr<ConnectionPool>
    pool() const
    {
        return pool_;
    }

private:
    std::shared_ptr<ConnectionPool> pool_;
};

}

#endif // !BACKEND_CONNECTION_DELETER_H_
