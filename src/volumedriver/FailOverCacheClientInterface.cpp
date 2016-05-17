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

#include "FailOverCacheClientInterface.h"
#include "FailOverCacheAsyncBridge.h"
#include "FailOverCacheSyncBridge.h"

namespace volumedriver
{

using Ptr = std::unique_ptr<FailOverCacheClientInterface>;

Ptr
FailOverCacheClientInterface::create(const FailOverCacheMode mode,
                                     const LBASize lba_size,
                                     const ClusterMultiplier cluster_multiplier,
                                     const size_t max_entries,
                                     const std::atomic<unsigned>& write_trigger)
{
    switch (mode)
    {
    case FailOverCacheMode::Asynchronous:
        return Ptr(new FailOverCacheAsyncBridge(lba_size,
                                                cluster_multiplier,
                                                max_entries,
                                                write_trigger));
    case FailOverCacheMode::Synchronous:
        return Ptr(new FailOverCacheSyncBridge(max_entries));
    }
}

} // namespace volumedriver
