// Copyright 2015 Open vStorage NV
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
