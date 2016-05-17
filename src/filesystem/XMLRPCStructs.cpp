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

#include "XMLRPCStructs.h"

namespace volumedriverfs
{

//Just borrowing boost's semi human readable xml serialization to support printing these objects in python.
//This is only intended for interactive python sessions (debugging/operations).
//Production client code should of course directly use the properties of the object.
// These are boilerplate!!
std::string
XMLRPCVolumeInfo::str() const
{
    std::stringstream ss;
    youtils::Serialization::serializeNVPAndFlush<oarchive_type>(ss,
                                                                serialization_name,
                                                                *this);
    return ss.str();
}

void
XMLRPCVolumeInfo::set_from_str(const std::string& str)
{
    std::stringstream ss(str);
    iarchive_type ia(ss);
    ia & boost::serialization::make_nvp(serialization_name,
                                        *this);
}

std::string
XMLRPCStatistics::str() const
{
    std::stringstream ss;
    youtils::Serialization::serializeNVPAndFlush<oarchive_type>(ss,
                                                                serialization_name,
                                                                *this);
    return ss.str();
}

void
XMLRPCStatistics::set_from_str(const std::string& str)
{
    std::stringstream ss(str);
    iarchive_type ia(ss);
    ia & boost::serialization::make_nvp(serialization_name,
                                        *this);
}

std::string
XMLRPCSnapshotInfo::str() const
{
    std::stringstream ss;
    youtils::Serialization::serializeNVPAndFlush<oarchive_type>(ss,
                                                                serialization_name,
                                                                *this);
    return ss.str();
}

void
XMLRPCSnapshotInfo::set_from_str(const std::string& str)
{
    std::stringstream ss(str);
    iarchive_type ia(ss);
    ia & boost::serialization::make_nvp(serialization_name,
                                        *this);
}

std::string
XMLRPCClusterCacheHandleInfo::str() const
{
    std::stringstream ss;
    youtils::Serialization::serializeNVPAndFlush<oarchive_type>(ss,
                                                                serialization_name,
                                                                *this);
    return ss.str();
}

void
XMLRPCClusterCacheHandleInfo::set_from_str(const std::string& str)
{
    std::stringstream ss(str);
    iarchive_type ia(ss);
    ia & boost::serialization::make_nvp(serialization_name,
                                        *this);
}

} // namespace volumedriverfs
