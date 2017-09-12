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

#ifndef BACKEND_ROUND_ROBIN_POOL_SELECTOR_H_
#define BACKEND_ROUND_ROBIN_POOL_SELECTOR_H_

#include <memory>

#include <youtils/Logging.h>

namespace backend
{

class BackendConnectionManager;
class ConnectionPool;

class RoundRobinPoolSelector
{
public:
    explicit RoundRobinPoolSelector(BackendConnectionManager&);

    ~RoundRobinPoolSelector() = default;

    RoundRobinPoolSelector(const RoundRobinPoolSelector&) = delete;

    RoundRobinPoolSelector&
    operator=(const RoundRobinPoolSelector&) = delete;

    const std::shared_ptr<ConnectionPool>&
    pool();

    void
    connection_error();

    void
    backend_error()
    {}

    void
    request_timeout()
    {}

private:
    DECLARE_LOGGER("RoundRobinPoolSelector");

    BackendConnectionManager& cm_;
    std::shared_ptr<ConnectionPool> pool_;
};

}

#endif // !BACKEND_ROUND_ROBIN_POOL_SELECTOR_H_
