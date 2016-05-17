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

#ifndef YT_HEARTBEAT_LOCK_H_
#define YT_HEARTBEAT_LOCK_H_

#include "Serialization.h"
#include "System.h"
#include "UUID.h"

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace youtils
{

class HeartBeatLock
{
public:
    using TimeDuration = boost::posix_time::time_duration;

    HeartBeatLock(const TimeDuration& session_timeout,
                  const TimeDuration& interrupt_timeout);

    HeartBeatLock(const std::string&);

    HeartBeatLock(const HeartBeatLock&) = default;

    HeartBeatLock&
    operator=(const HeartBeatLock&) = default;

    void
    operator++();

    void
    save(std::string&) const;

    bool
    same_owner(const HeartBeatLock&) const;

    bool
    different_owner(const HeartBeatLock&) const;

    TimeDuration
    get_timeout() const;

    bool
    hasLock() const
    {
        return has_lock_;
    }

    void
    hasLock(bool has_lock)
    {
        has_lock_ = has_lock;
    }

    TimeDuration
    session_timeout() const
    {
        return session_timeout_;
    }

    TimeDuration
    interrupt_timeout() const
    {
        return interrupt_timeout_;
    }

private:
    UUID uuid;
    bool has_lock_;
    uint64_t counter;
    TimeDuration session_timeout_;
    TimeDuration interrupt_timeout_;

    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if(version == 0)
        {
            ar & BOOST_SERIALIZATION_NVP(uuid);
            ar & boost::serialization::make_nvp("hasLock",
                                                has_lock_);
            ar & BOOST_SERIALIZATION_NVP(counter);
            uint64_t session_timeout_milliseconds;
            ar & BOOST_SERIALIZATION_NVP(session_timeout_milliseconds);
            session_timeout_ = boost::posix_time::milliseconds(session_timeout_milliseconds);
            uint64_t interrupt_timeout_milliseconds;
            ar & BOOST_SERIALIZATION_NVP(interrupt_timeout_milliseconds);
            interrupt_timeout_ = boost::posix_time::milliseconds(interrupt_timeout_milliseconds);
            std::string hostname;
            uint64_t pid;
            ar & BOOST_SERIALIZATION_NVP(hostname);
            ar & BOOST_SERIALIZATION_NVP(pid);
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version,0,0);
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if(version == 0)
        {

            ar & BOOST_SERIALIZATION_NVP(uuid);
            ar & boost::serialization::make_nvp("hasLock",
                                                has_lock_);
            ar & BOOST_SERIALIZATION_NVP(counter);
            uint64_t session_timeout_milliseconds = session_timeout_.total_milliseconds();
            ar & BOOST_SERIALIZATION_NVP(session_timeout_milliseconds);
            uint64_t interrupt_timeout_milliseconds  = interrupt_timeout_.total_milliseconds();
            ar & BOOST_SERIALIZATION_NVP(interrupt_timeout_milliseconds);
            std::string hostname = System::getHostName();
            uint64_t pid = System::getPID();
            ar & BOOST_SERIALIZATION_NVP(hostname);
            ar & BOOST_SERIALIZATION_NVP(pid);
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version,0,0);
        }
    }
};

}

BOOST_CLASS_VERSION(youtils::HeartBeatLock, 0);

#endif // YT_HEARTBEAT_LOCK_H_


// Local Variables: **
// mode: c++ **
// End: **
