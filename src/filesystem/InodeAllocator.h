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
