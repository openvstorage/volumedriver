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

#ifndef VD_FAILOVER_CACHE_ENTRY_H_
#define VD_FAILOVER_CACHE_ENTRY_H_

#include "ClusterLocation.h"

namespace volumedriver
{

struct FailOverCacheEntry
{
    FailOverCacheEntry(ClusterLocation cli,
                       uint64_t lba,
                       const uint8_t* buffer,
                       uint32_t size)
        : cli_(cli)
        , lba_(lba)
        , size_(size)
        , buffer_(buffer)
    {}

    ClusterLocation cli_;
    uint64_t lba_;
    uint32_t size_;
    const uint8_t* buffer_;
};

}

#endif // !VD_FAILOVER_CACHE_ENTRY_H_
