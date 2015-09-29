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
