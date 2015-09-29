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
static_assert(sizeof(ClusterLocationAndHash)== 24, "Unexpected size for ClusterAddress");
static_assert(sizeof(Entry) == 32, "Unexpected size for Entry");

}

// Local Variables: **
// mode: c++ **
// End: **
