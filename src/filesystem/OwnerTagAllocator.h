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

#ifndef VFS_OWNER_TAG_ALLOCATOR_H_
#define VFS_OWNER_TAG_ALLOCATOR_H_

#include "LockedArakoonCounter.h"

#include <youtils/Logging.h>

#include <volumedriver/OwnerTag.h>

namespace arakoon
{

template<>
struct DataBufferTraits<volumedriver::OwnerTag>
{
    static size_t
    size(const volumedriver::OwnerTag& tag)
    {
        return sizeof(tag);
    }

    static const void*
    data(const volumedriver::OwnerTag& tag)
    {
        return &tag;
    }

    static volumedriver::OwnerTag
    deserialize(void* data,
                size_t size)
    {
        using namespace volumedriver;

        THROW_UNLESS(sizeof(OwnerTag) == size);
        OwnerTag tag(OwnerTag(*static_cast<const OwnerTag*>(data)));
        ::free(data);
        return tag;
    }
};

}

namespace volumedriverfs
{

template<>
struct LockedArakoonCounterTraits<volumedriver::OwnerTag>
{
    static const char*
    key()
    {
        static const char* k = "last_owner_tag";
        return k;
    }

    static volumedriver::OwnerTag
    initial_value()
    {
        return volumedriver::OwnerTag(1);
    }

    static volumedriver::OwnerTag
    update_value(const volumedriver::OwnerTag old)
    {
        using namespace volumedriver;

        OwnerTag new_tag(old + 1);
        if (new_tag == OwnerTag(0))
        {
            return OwnerTag(1);
        }
        else
        {
            return new_tag;
        }
    }
};

using OwnerTagAllocator = LockedArakoonCounter<volumedriver::OwnerTag>;

}

#endif // !VFS_OWNER_TAG_ALLOCATOR_H_
