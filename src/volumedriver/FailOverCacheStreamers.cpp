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

#include "FailOverCacheStreamers.h"
#include "VolumeConfig.h"

#include "failovercache/fungilib/IOBaseStream.h"
#include "failovercache/fungilib/WrapByteArray.h"
#include "failovercache/fungilib/Socket.h"

#include <iostream>

namespace
{

DECLARE_LOGGER("FailOverCacheStreamers");

}

namespace volumedriver
{

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream,
           FailOverCacheCommand cmd)
{
    return stream << static_cast<uint32_t>(cmd);
}

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream,
           FailOverCacheCommand& cmd)
{
    return stream >> reinterpret_cast<uint32_t&>(cmd);
}

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream,
           const CommandData<FailOverCacheCommand::RemoveUpTo>& data)
{
    return stream << data.sco;
}

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream,
           CommandData<FailOverCacheCommand::RemoveUpTo>& data)
{
    return stream >> data.sco;
}

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream,
           const CommandData<FailOverCacheCommand::GetSCO>& data)
{
    return stream << data.sco;
}

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream,
           CommandData<FailOverCacheCommand::GetSCO>& data)
{
    return stream >> data.sco;
}

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream,
           const CommandData<FailOverCacheCommand::Register>& data)
{
    stream << data.ns_;
    return stream << static_cast<uint32_t>(data.clustersize_);
}

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream,
           CommandData<FailOverCacheCommand::Register>& data)
{
    stream >> data.ns_;
    stream >> data.clustersize_;
    return stream;
}

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& stream,
           const CommandData<FailOverCacheCommand::AddEntries>& data)
{
    if (not data.entries_.empty())
    {
        VERIFY(data.entries_.front().cli_.sco() == data.entries_.back().cli_.sco());
    }

    // TODO: AR: try to get rid of this and the corking business
    bool isRDMA = stream.isRdma();

    const size_t wsize = data.entries_.size();
    stream << wsize;

    if (isRDMA) // make small but finished packets with RDMA
    {
        stream << fungi::IOBaseStream::uncork;
    }

    for (const auto& entry : data.entries_)
    {
        if (isRDMA) // make small but finished packets with RDMA
        {
            stream << fungi::IOBaseStream::cork;
        }
        stream << entry.cli_;
        stream << entry.lba_;
        TODO("ArneT: funky cast...");
        stream << fungi::WrapByteArray((byte*) entry.buffer_, entry.size_);
        if (isRDMA) // make small but finished packets with RDMA
        {
            stream << fungi::IOBaseStream::uncork;
        }
    }

    return stream;
}

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream,
           CommandData<FailOverCacheCommand::AddEntries>& data)
{
    size_t count = 0;
    stream >> count;

    data.entries_.reserve(count);

    uint64_t cluster_size = 0;
    uint8_t* ptr = nullptr;

    for(size_t i = 0; i < count; ++i)
    {
        ClusterLocation cli;
        stream >> cli;

        uint64_t lba;
        stream >> lba;

        int64_t sz;
        stream >> sz;
        VERIFY(sz != 0);

        if (ptr == nullptr)
        {
            VERIFY(data.buf_ == nullptr);

            cluster_size = sz;
            data.buf_ = std::make_unique<uint8_t[]>(cluster_size * count);
            ptr = data.buf_.get();
        }
        else
        {
            VERIFY(static_cast<int64_t>(cluster_size) == sz);
            VERIFY(data.buf_ != nullptr);
        }

        stream.readIntoByteArray(ptr,
                                 cluster_size);
        data.entries_.emplace_back(cli,
                                   lba,
                                   ptr,
                                   cluster_size);
        ptr += cluster_size;
    }

    if (not data.entries_.empty())
    {
        VERIFY(data.entries_.front().cli_.sco() == data.entries_.back().cli_.sco());
    }

    return stream;
}

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& stream,
           CommandData<FailOverCacheCommand::TunnelCapnProto>& data)
{
    stream >> data.hdr.tag;
    stream >> data.hdr.capnp_size;
    stream >> data.hdr.data_size;

    return stream;
}

}


// Local Variables: **
// mode: c++ **
// End: **
