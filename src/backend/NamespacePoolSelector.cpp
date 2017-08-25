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

#include "ConnectionPool.h"
#include "BackendConnectionManager.h"
#include "NamespacePoolSelector.h"

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

NamespacePoolSelector::NamespacePoolSelector(const BackendConnectionManager& cm,
                                             const Namespace& ns)
    : cm_(cm)
    , nspace_(ns)
    , idx_(std::hash<std::string>()(nspace_.str()) % cm_.pools().size())
    , last_idx_(idx_)
    , start_from_last_(false)
{}

const std::shared_ptr<ConnectionPool>&
NamespacePoolSelector::pool()
{
    const BackendConnectionManager::ConnectionPools& pools = cm_.pools();
    ASSERT(not pools.empty());

    const size_t idx = start_from_last_ ? last_idx_ : idx_;
    size_t i = idx;

    while (true)
    {
        ASSERT(i < pools.size());
        if (not pools[i]->blacklisted())
        {
            break;
        }
        else
        {
            ++i;
            if (i == pools.size())
            {
                i = 0;
            }

            if (i == idx)
            {
                // all pools are blacklisted
                i = random_idx(pools.size() - 1);
                break;
            }
        }
    }

    last_idx_ = i;
    return pools[i];
}

void
NamespacePoolSelector::connection_error()
{
    VERIFY(cm_.pools().size() > last_idx_);
    cm_.pools()[last_idx_]->error();
}

void
NamespacePoolSelector::backend_error()
{
    if (cm_.connection_manager_parameters().backend_interface_switch_connection_pool_on_error.value())
    {
        start_from_last_ = true;
        last_idx_ = (last_idx_ + 1) % cm_.pools().size();
    }
}

}
