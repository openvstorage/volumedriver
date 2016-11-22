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

#ifndef __NETWORK_XIO_MEMPOOL_H_
#define __NETWORK_XIO_MEMPOOL_H_

#include "NetworkXioCommon.h"
#include "NetworkXioMempoolSlab.h"

#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace volumedriverfs
{

MAKE_EXCEPTION(FailedCreateMempoolThread, fungi::IOException);
MAKE_EXCEPTION(FailedCreateMempoolSlab, fungi::IOException);

class NetworkXioMempool
{
public:
    NetworkXioMempool();

    ~NetworkXioMempool();

    slab_mem_block*
    alloc(size_t length);

    void
    free(slab_mem_block *mem_block);

    void
    add_slab(size_t block_size,
             size_t min,
             size_t max,
             size_t quantum);
private:
    DECLARE_LOGGER("NetworkXioMempool");

    std::map<uint64_t, NetworkXioMempoolSlabPtr> slabs;
    uint64_t slab_index;

    std::thread slabs_manager_thread;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping;

    void
    slabs_manager();
};

typedef std::shared_ptr<NetworkXioMempool> NetworkXioMempoolPtr;

} //namespace

#endif //
