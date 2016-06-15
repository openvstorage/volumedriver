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

#ifndef META_DATA_STORE_STATS_H_
#define META_DATA_STORE_STATS_H_

// this file is part of the volumedriver api
#include <stdint.h>

#include <vector>

#include <youtils/UUID.h>

namespace volumedriver
{

struct MetaDataStoreStats
{
    MetaDataStoreStats()
        : cache_hits(0)
        , cache_misses(0)
        , used_clusters(0)
        , max_pages(0)
        , cached_pages(0)
    {}

    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t used_clusters;
    uint32_t max_pages;
    uint32_t cached_pages;
    // contains also discarded clusters!
    std::vector< std::pair<youtils::UUID, uint64_t> > corked_clusters;
};

}

#endif // !META_DATA_STORE_STATS_H_

// namespace volumedriver
// Local Variables: **
// mode: c++ **
// End: **
