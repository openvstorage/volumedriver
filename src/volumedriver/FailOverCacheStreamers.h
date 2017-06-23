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

#ifndef FAILOVERCACHESTREAMERS_H
#define FAILOVERCACHESTREAMERS_H

#include "FailOverCacheCommand.h"
// a fwd declaration doesn't cut it even though we only
// deal with vector<FailOverCacheEntry>
#include "FailOverCacheEntry.h"
#include "Types.h"

#include <vector>

namespace fungi
{

class IOBaseStream;

}

namespace volumedriver
{

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream&,
           FailOverCacheCommand);

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream&,
           FailOverCacheCommand&);

template<FailOverCacheCommand cmd>
struct CommandData;

template<>
struct CommandData<FailOverCacheCommand::RemoveUpTo>
{
    SCO sco;

    explicit CommandData(const SCO s)
        : sco(s)
    {}
};

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream&,
           const CommandData<FailOverCacheCommand::RemoveUpTo>&);

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream&,
           CommandData<FailOverCacheCommand::RemoveUpTo>&);

template<>
struct CommandData<FailOverCacheCommand::GetSCO>
{
    SCO sco;

    explicit CommandData(const SCO s)
        : sco(s)
    {}
};

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream&,
           const CommandData<FailOverCacheCommand::GetSCO>&);

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream&,
           CommandData<FailOverCacheCommand::GetSCO>&);

template<>
struct CommandData<FailOverCacheCommand::Register>
{
    explicit CommandData(const std::string& ns = "",
                         ClusterSize clustersize = ClusterSize(0))
        : ns_(ns)
        , clustersize_(clustersize)
    {}

    std::string ns_;
    ClusterSize clustersize_;
};

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream&,
           const CommandData<FailOverCacheCommand::Register>&);

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream&,
           CommandData<FailOverCacheCommand::Register>&);

template<>
struct CommandData<FailOverCacheCommand::AddEntries>
{
    using EntryVector = std::vector<FailOverCacheEntry>;

    // Only used when streaming in
    CommandData(EntryVector entries)
        : entries_(std::move(entries))
    {}

    // Only used when streaming in
    std::unique_ptr<uint8_t[]> buf_;

    // Only used when streaming out
    CommandData() = default;

    EntryVector entries_;
};

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream&,
           const CommandData<FailOverCacheCommand::AddEntries>&);

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream&,
           CommandData<FailOverCacheCommand::AddEntries>&);

template<>
struct CommandData<FailOverCacheCommand::TunnelCapnProto>
{
    CommandData(TunnelCapnProtoHeader& h)
        : hdr(h)
    {}

    TunnelCapnProtoHeader& hdr;
};

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream&,
           CommandData<FailOverCacheCommand::TunnelCapnProto>&);
}

#endif // FAILOVERCACHESTREAMERS_H

// Local Variables: **
// mode: c++ **
// End: **
