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
#ifndef __SHM_VOLUME_CACHE_H
#define __SHM_VOLUME_CACHE_H

#include "ShmSegmentDetails.h"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <set>
#include <memory>

namespace volumedriverfs
{
namespace ipc = boost::interprocess;

class ShmVolumeCache
{
public:
    explicit ShmVolumeCache(const ShmSegmentDetails& segment_details)
    {
        try
        {
            shm_segment_ = ipc::managed_shared_memory(ipc::open_only,
                                                      segment_details.id().c_str());
        }
        catch (ipc::interprocess_exception&)
        {
            LOG_FATAL("cannot open shared memory segment");
            throw;
        }
    }

    ~ShmVolumeCache()
    {
        _drop_mempool();
    }

    ipc::managed_shared_memory::handle_t
    allocate(size_t size)
    {
        void *ptr = shm_segment_.allocate(size);
        auto ret = mempool_.insert(ptr);
        VERIFY(ret.second == true);
        return shm_segment_.get_handle_from_address(*ret.first);
    }

    bool
    deallocate(ipc::managed_shared_memory::handle_t handle)
    {
        void *ptr = shm_segment_.get_address_from_handle(handle);
        if (ptr)
        {
            auto it = mempool_.find(ptr);
            if (it != mempool_.end())
            {
                shm_segment_.deallocate(ptr);
                mempool_.erase(it);
                return true;
            }
        }
        return false;
    }

private:
    DECLARE_LOGGER("ShmVolumeCache");

    ipc::managed_shared_memory shm_segment_;
    std::set<void*> mempool_;

    void
    _drop_mempool()
    {
        for (auto& x: mempool_)
        {
            shm_segment_.deallocate(x);
        }
        mempool_.clear();
    }
};

typedef std::unique_ptr<ShmVolumeCache> ShmVolumeCachePtr;

} //namespace volumedriverfs

#endif
