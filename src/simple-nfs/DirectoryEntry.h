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

#ifndef SIMPLE_NFS_DIRECTORY_ENTRY_H_
#define SIMPLE_NFS_DIRECTORY_ENTRY_H_

#include <cstdint>
#include <string>

namespace simple_nfs
{

enum class EntryType
: uint32_t
{
    NF3REG = 1,
    NF3DIR = 2,
    NF3BLK = 3,
    NF3CHR = 4,
    NF3LNK = 5,
    NF3SOCK = 6,
    NF3FIFO = 7,
};

struct DirectoryEntry
{
    DirectoryEntry(const char* nname,
                   uint64_t iinode,
                   uint32_t ttype,
                   uint32_t mmode,
                   uint64_t ssize,
                   uint32_t uuid,
                   uint32_t ggid)
        : name(nname)
        , inode(iinode)
        , type(static_cast<EntryType>(ttype))
        , mode(mmode)
        , size(ssize)
        , uid(uuid)
        , gid(ggid)
    {}

    const std::string name;
    const uint64_t inode;
    const EntryType type;
    const uint32_t mode;
    const uint64_t size;
    const uint32_t uid;
    const uint32_t gid;
};

}

#endif // !SIMPLE_NFS_DIRECTORY_ENTRY_H_
