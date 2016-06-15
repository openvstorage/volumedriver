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

#include "Entry.h"

#include <cstring>

#include <youtils/Assert.h>

namespace volumedriver
{

Entry::Entry()
    : clusteraddress_(0)
{}

Entry::Entry(const ClusterAddress& ca,
             const ClusterLocationAndHash& loc)
    : clusteraddress_(ca)
    , loc_and_hash_(loc)
{
    VERIFY(not loc_and_hash_.clusterLocation.isNull());
    THROW_UNLESS(clusteraddress_ <= max_valid_cluster_address());
}

Entry::Entry(const CheckSum& checksum,
             Entry::Type type)
    : clusteraddress_(checksum.getValue() bitor
             (static_cast<uint64_t>(type) << checksum_shift_))
{
    VERIFY(type == Type::TLogCRC or
           type == Type::SCOCRC);
}

ClusterAddress
Entry::clusterAddress() const
{
    if (not loc_and_hash_.clusterLocation.isNull())
    {
        THROW_UNLESS(clusteraddress_ <= max_valid_cluster_address());
    }

    return clusteraddress_;
}

const ClusterLocationAndHash&
Entry::clusterLocationAndHash() const
{
    return loc_and_hash_;
}

ClusterLocation
Entry::clusterLocation() const
{
    return loc_and_hash_.clusterLocation;
}

Entry::Type
Entry::type() const
{
    if (not loc_and_hash_.clusterLocation.isNull())
    {
        return Type::LOC;
    }
    else
    {
        const uint64_t crc_type = clusteraddress_ >> checksum_shift_;
        if (crc_type == static_cast<uint64_t>(Type::TLogCRC))
        {
            return Type::TLogCRC;
        }
        else if (crc_type == static_cast<uint64_t>(Type::SCOCRC))
        {
            return Type::SCOCRC;
        }

        if (clusteraddress_ == static_cast<ClusterAddress>(Type::SyncTC))
        {
            return Type::SyncTC;
        }
    }

    // The entry is rubbish - let's take some time to assemble a nice message.
    std::stringstream ss;
    ss << "invalid Entry: ca " << clusteraddress_ << ", loc " << loc_and_hash_;
    throw InvalidEntryException(ss.str().c_str());
}

bool operator==(const Entry& x, const Entry& y)
{
        return x.clusterAddress() == y.clusterAddress() and
            x.clusterLocation() == y.clusterLocation();
}

static_assert(sizeof(ClusterAddress)== 8, "Unexpected size for ClusterAddress");
static_assert(sizeof(Entry) == sizeof(ClusterAddress) + sizeof(ClusterLocationAndHash),
              "Unexpected size for Entry");

}

// Local Variables: **
// mode: c++ **
// End: **
