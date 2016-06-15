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
