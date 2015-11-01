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
