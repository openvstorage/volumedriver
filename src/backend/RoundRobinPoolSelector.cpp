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

#include "BackendConnectionManager.h"
#include "ConnectionPool.h"
#include "RoundRobinPoolSelector.h"

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>

#include <youtils/SourceOfUncertainty.h>

namespace backend
{

namespace
{

size_t
random_idx(size_t max)
{
    static boost::mutex rand_lock;

    boost::lock_guard<decltype(rand_lock)> g(rand_lock);
    static youtils::SourceOfUncertainty rand;

    return rand(max);
}

}

RoundRobinPoolSelector::RoundRobinPoolSelector(BackendConnectionManager& cm)
    : cm_(cm)
{}

const std::shared_ptr<ConnectionPool>&
RoundRobinPoolSelector::pool()
{
    const BackendConnectionManager::ConnectionPools& pools = cm_.pools();
    ASSERT(not pools.empty());

    for (size_t i = 0; i < pools.size(); ++i)
    {
        pool_ = cm_.round_robin_select_();
        if (not pool_->blacklisted())
        {
            return pool_;
        }
    }

    size_t i = random_idx(pools.size() - 1);
    VERIFY(i < pools.size());

    return pools[i];
}

void
RoundRobinPoolSelector::connection_error()
{
    VERIFY(pool_);
    pool_->error();
}

}
