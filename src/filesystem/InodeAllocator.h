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

#ifndef VFS_INODE_ALLOCATOR_H_
#define VFS_INODE_ALLOCATOR_H_

#include "DirectoryEntry.h"
#include "LockedArakoonCounter.h"

#include <youtils/LockedArakoon.h>
#include <youtils/Logging.h>

namespace arakoon
{

template<>
struct DataBufferTraits<volumedriverfs::Inode>
{
    static size_t
    size(const volumedriverfs::Inode& inode)
    {
        return sizeof(inode);
    }

    static const void*
    data(const volumedriverfs::Inode& inode)
    {
        return &inode;
    }

    static volumedriverfs::Inode
    deserialize(void* data,
                size_t size)
    {
        using namespace volumedriverfs;

        THROW_UNLESS(sizeof(Inode) == size);
        Inode i(Inode(*static_cast<const Inode*>(data)));
        ::free(data);
        return i;
    }
};

}

namespace volumedriverfs
{

template<>
struct LockedArakoonCounterTraits<Inode>
{
    static const char*
    key()
    {
        static const char* k = "last_inode";
        return k;
    }

    static Inode
    initial_value()
    {
        return Inode(1);
    }

    static Inode
    update_value(const Inode old)
    {
        return Inode(old + 1);
    }
};

using InodeAllocator = LockedArakoonCounter<Inode>;

}

#endif // !VFS_INODE_ALLOCATOR_H_
