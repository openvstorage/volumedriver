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

#ifndef __NETWORK_XIO_MEMPOOL_SLAB_H_
#define __NETWORK_XIO_MEMPOOL_SLAB_H_

#include "NetworkXioCommon.h"

#include <youtils/SpinLock.h>
#include <boost/thread/lock_guard.hpp>
#include <boost/intrusive/list.hpp>

#include <map>

namespace volumedriverfs
{

class NetworkXioMempoolSlab
{
public:
    explicit NetworkXioMempoolSlab(size_t block_size,
                                   size_t min_size,
                                   size_t max_size,
                                   size_t quantum_size,
                                   uint64_t index);
    ~NetworkXioMempoolSlab();

    slab_mem_block*
    alloc();

    void
    free(slab_mem_block *mem_block);

    void
    try_free_unused_blocks();

    uint64_t
    get_index() const
    {
        return slab_index;
    }

    size_t
    get_mb_size() const
    {
        return mb_size;
    }
private:
    DECLARE_LOGGER("NetworkXioMempoolSlab");

    struct Region
    {
        explicit Region(size_t nr_blocks,
                        size_t mb_size,
                        uint64_t index)
        : items(nr_blocks)
        , region_index(index)
        , refcnt(0)
        {
            int ret = xio_mem_alloc(mb_size * nr_blocks, &region_reg_mem);
            if (ret < 0)
            {
                throw std::bad_alloc();
            }
        };
        size_t items;
        xio_reg_mem region_reg_mem;
        uint64_t region_index;
        uint64_t refcnt;
    };

    typedef std::unique_ptr<Region> RegionPtr;
    typedef boost::intrusive::list<slab_mem_block> BlocksList;
    typedef std::shared_ptr<BlocksList> RegionBlocksBucketPtr;

    size_t used_blocks;
    size_t total_blocks;
    size_t mb_size;
    size_t min;
    size_t max;
    size_t growing_quantum;
    uint64_t slab_index;
    uint64_t region_index;
    uint64_t min_slab_index;

    fungi::SpinLock lock;
    std::map<uint64_t, RegionPtr> regions;
    std::map<uint64_t, RegionBlocksBucketPtr> blocks;

    void
    allocate_new_blocks(size_t nr_blocks);

    void
    free_regions_locked();

    slab_mem_block*
    try_alloc_block();
};

typedef std::unique_ptr<NetworkXioMempoolSlab> NetworkXioMempoolSlabPtr;

} //namespace volumedriverfs

#endif //__NETWORK_XIO_MEMPOOL_SLAB_H_

