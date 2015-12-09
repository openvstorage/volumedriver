// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef __SHM_VOLUME_CACHE_H
#define __SHM_VOLUME_CACHE_H

#include "ShmCommon.h"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <set>
#include <memory>

namespace volumedriverfs
{
namespace ipc = boost::interprocess;

class ShmVolumeCache
{
public:
    ShmVolumeCache()
    {
        try
        {
            shm_segment_ = ipc::managed_shared_memory(ipc::open_only,
                                                      ShmSegmentDetails::Name());
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
