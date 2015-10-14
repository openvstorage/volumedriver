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


#include "HeartBeatLock.h"

namespace youtils
{

HeartBeatLock::HeartBeatLock(const TimeDuration& session_timeout,
                             const TimeDuration& interrupt_timeout)
    : has_lock_(true)
    , counter(0)
    , session_timeout_(session_timeout)
    , interrupt_timeout_(interrupt_timeout)
{}

HeartBeatLock::HeartBeatLock(const std::string& str)
    : has_lock_(true)
{
    std::stringstream ss(str);
    boost::archive::xml_iarchive ia(ss);
    ia >> boost::serialization::make_nvp("lock",
                                         *this);
}

void
HeartBeatLock::operator++()
{
    ++counter;
}

void
HeartBeatLock::save(std::string& str) const
{
    std::stringstream ss;
    boost::archive::xml_oarchive oa(ss);
    oa << boost::serialization::make_nvp("lock",
                                         *this);
    str = ss.str();
}

bool
HeartBeatLock::same_owner(const HeartBeatLock& inOther) const
{
    return uuid == inOther.uuid;
}

bool
HeartBeatLock::different_owner(const HeartBeatLock& inOther) const
{
    return uuid != inOther.uuid;
}

HeartBeatLock::TimeDuration
HeartBeatLock::get_timeout() const
{
    return boost::posix_time::milliseconds(3* session_timeout_.total_milliseconds()) + interrupt_timeout_;
}

}

// Local Variables: **
// mode: c++ **
// End: **
