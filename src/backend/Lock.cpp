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


#include "Lock.h"

namespace backend
{

Lock::Lock(const boost::posix_time::time_duration& session_timeout,
           const boost::posix_time::time_duration& interrupt_timeout)
    : hasLock(true)
    , counter(0)
    , session_timeout_(session_timeout)
    , interrupt_timeout_(interrupt_timeout)
{}

Lock::Lock(const std::string& str)
    : hasLock(true)
{
    std::stringstream ss(str);
    boost::archive::xml_iarchive ia(ss);
    ia >> boost::serialization::make_nvp("lock",
                                         *this);
}

void
Lock::operator++()
{
    ++counter;
}

void
Lock::save(std::string& str) const
{
    std::stringstream ss;
    boost::archive::xml_oarchive oa(ss);
    oa << boost::serialization::make_nvp("lock",
                                         *this);
    str = ss.str();
}

bool
Lock::same_owner(const Lock& inOther) const
{
    return uuid == inOther.uuid;
}

bool
Lock::different_owner(const Lock& inOther) const
{
    return uuid != inOther.uuid;
}

boost::posix_time::time_duration
Lock::get_timeout() const
{
    return boost::posix_time::milliseconds(3* session_timeout_.total_milliseconds()) + interrupt_timeout_;
}

}

// Local Variables: **
// bvirtual-targets: ("bin/backup_volume_driver") **
// mode: c++ **
// End: **
