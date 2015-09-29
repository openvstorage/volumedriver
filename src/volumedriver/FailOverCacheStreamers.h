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

#ifndef FAILOVERCACHESTREAMERS_H
#define FAILOVERCACHESTREAMERS_H

#include "failovercache/fungilib/IOBaseStream.h"
#include <youtils/IOException.h>
#include "Types.h"
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_array.hpp>
#include "ClusterLocation.h"
namespace volumedriver
{

enum FailOverCacheCommand
{
    RemoveUpTo     =  0x1,
    GetSCO         =  0x2,
    Register       =  0x3,
    AddEntries     =  0x4,
    GetEntries     =  0x5,
    Flush          =  0x6,
    Ok             =  0x7,
    NotOk          =  0x8,
    Unregister     =  0x9,
    Clear          =  0xA,
    GetSCORange    =  0xB
    //    Bye            =  0xff
};

#define OUT_ENUM(stream, val)                   \
    {                                           \
        const uint32_t com = val;               \
        stream << com;                          \
    }                                           \


template<FailOverCacheCommand val>
struct CommandData;

template<>
struct CommandData<Register>
{
    CommandData(const std::string& ns = "",
                volumedriver::ClusterSize clustersize = ClusterSize(0))
        : ns_(ns)
        , clustersize_(clustersize)
    {}

    std::string ns_;
    ClusterSize clustersize_;
};

fungi::IOBaseStream&
checkStreamOK(fungi::IOBaseStream& stream,
              const std::string& ex);


fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream, const CommandData<Register>& data);


fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream, CommandData<Register>& data);

struct FailOverCacheEntry
{

    FailOverCacheEntry(ClusterLocation cli,
                       uint64_t lba,
                       const uint8_t* buffer,
                       uint32_t size)
        : cli_(cli)
        , lba_(lba)
        , buffer_(new byte[size])
        , size_(size)
    {
        memcpy(&buffer_[0], buffer, size_);
    }

    FailOverCacheEntry(ClusterLocation cli,
                       uint64_t lba,
                       byte* buffer,
                       uint32_t size)
        : cli_(cli)
        , lba_(lba)
        , buffer_(buffer)
        , size_(size)
    {}

    ~FailOverCacheEntry()
    {
    }


    FailOverCacheEntry()
        : lba_(0)
        , buffer_(0)

    {}

    ClusterLocation cli_;
    uint64_t lba_;
    boost::scoped_array<byte> buffer_;
    uint32_t size_;
};


template<>
struct CommandData<AddEntries>

{
    CommandData(boost::ptr_vector<FailOverCacheEntry>& entries)
        : entries_(entries)
    {}

    boost::ptr_vector<FailOverCacheEntry>& entries_;
};

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream, const CommandData<AddEntries>& data);


fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream, CommandData<AddEntries>& data);


template<>
struct CommandData<Flush>
{};

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream, const CommandData<Flush>& /*data*/);

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream, CommandData<Flush>& /*data*/);

template<>
struct CommandData<Clear>
{};

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream, const CommandData<Clear>& /*data*/);


}

#endif // FAILOVERCACHESTREAMERS_H

// Local Variables: **
// mode: c++ **
// End: **
