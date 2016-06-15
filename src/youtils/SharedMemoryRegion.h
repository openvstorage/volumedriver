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
