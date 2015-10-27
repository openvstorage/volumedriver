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

#ifndef YOUTILS_SHARED_MEMORY_REGION_H_
#define YOUTILS_SHARED_MEMORY_REGION_H_

#include "Logging.h"
#include "SharedMemoryRegionId.h"

#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

namespace youtils
{

class SharedMemoryRegion
{
public:
    explicit SharedMemoryRegion(size_t size);

    explicit SharedMemoryRegion(SharedMemoryRegionId id);

    ~SharedMemoryRegion();

    SharedMemoryRegion(const SharedMemoryRegion&) = delete;

    SharedMemoryRegion(SharedMemoryRegion&&);

    SharedMemoryRegion&
    operator=(const SharedMemoryRegion&) = delete;

    void*
    address() const
    {
        return region_.get_address();
    }

    std::size_t
    size() const
    {
        return region_.get_size();
    }

    const SharedMemoryRegionId&
    id() const
    {
        return id_;
    }

private:
    DECLARE_LOGGER("MetaDataServerSharedMemoryRegion");

    boost::interprocess::shared_memory_object object_;
    boost::interprocess::mapped_region region_;
    const SharedMemoryRegionId id_;
    bool owner_ = true;
};

}

#endif // !YOUTILS_SHARED_MEMORY_REGION_H_
