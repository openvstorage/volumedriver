// Copyright (C) 2017 iNuron NV
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

#ifndef BACKEND_CONNECTION_POOL_SELECTOR_INTERFACE_H_
#define BACKEND_CONNECTION_POOL_SELECTOR_INTERFACE_H_

#include <memory>

namespace backend
{

class ConnectionPool;

struct ConnectionPoolSelectorInterface
{
    virtual ~ConnectionPoolSelectorInterface() = default;

    virtual const std::shared_ptr<ConnectionPool>&
    pool() = 0;

    virtual void
    connection_error() = 0;

    virtual void
    backend_error() = 0;

    virtual void
    request_timeout() = 0;
};

}

#endif // !BACKEND_CONNECTION_POOL_SELECTOR_INTERFACE_H_
