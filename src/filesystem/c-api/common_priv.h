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

#ifndef __LIB_OVS_COMMON_PRIV_H
#define __LIB_OVS_COMMON_PRIV_H

#include <string>
#include <vector>
#include <map>

namespace libovsvolumedriver
{

struct ClusterLocation
{
    ClusterLocation()
    : offset_(0)
    , version_(0)
    , number_(0)
    , clone_id_(0)
    {}

    ClusterLocation(uint32_t num_,
                    uint16_t off_ = 0,
                    uint8_t cl_ = 0,
                    uint8_t ver_ = 0)
    : offset_(off_)
    , version_(ver_)
    , number_(num_)
    , clone_id_(cl_)
    {}

    uint16_t offset() const
    {
        return offset_;
    }

    uint8_t version() const
    {
        return version_;
    }

    uint32_t number() const
    {
        return number_;
    }

    uint8_t clone_id() const
    {
        return clone_id_;
    }

    bool
    operator==(const ClusterLocation& other) const
    {
        return offset_ == other.offset_ and
               version_ == other.version_ and
               number_ == other.number_ and
               clone_id_ == other.clone_id_;
    }

    uint16_t offset_;
    uint8_t version_;
    uint32_t number_;
    uint8_t clone_id_;
} __attribute__((__packed__));

typedef uint64_t ClusterAddress;
typedef std::vector<ClusterLocation> ClusterLocationPage;
typedef std::map<uint8_t, std::string> CloneNamespaceMap;

} //namespace libovsvolumedriver

#endif //__LIB_OVS_COMMON_PRIV_H
