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

#ifndef BACKEND_LOCK_H_
#define BACKEND_LOCK_H_

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <youtils/Serialization.h>
#include <youtils/System.h>
#include <youtils/UUID.h>

namespace volumedrivertest
{

class BackwardsCompatibilityTest;

}

namespace backend
{

class LockCommunicator;

class Lock
{
    friend class LockCommunicator;
    friend class volumedrivertest::BackwardsCompatibilityTest;
public:
    Lock(const boost::posix_time::time_duration& session_timeout,
             const boost::posix_time::time_duration& interrupt_timeout);

    Lock(const std::string& str);

    Lock(const Lock&) = default;

    Lock&
    operator=(const Lock&) = default;

    void
    operator++();

    void
    save(std::string& str) const;

    bool
    same_owner(const Lock& inOther) const;

    bool
    different_owner(const Lock& inOther) const;

    boost::posix_time::time_duration
    get_timeout() const;

private:
    youtils::UUID uuid;
    bool hasLock;
    uint64_t counter;
    boost::posix_time::time_duration session_timeout_;
    boost::posix_time::time_duration interrupt_timeout_;

    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if(version == 0)
        {
            ar & BOOST_SERIALIZATION_NVP(uuid);
            ar & BOOST_SERIALIZATION_NVP(hasLock);
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
            ar & BOOST_SERIALIZATION_NVP(hasLock);
            ar & BOOST_SERIALIZATION_NVP(counter);
            uint64_t session_timeout_milliseconds = session_timeout_.total_milliseconds();
            ar & BOOST_SERIALIZATION_NVP(session_timeout_milliseconds);
            uint64_t interrupt_timeout_milliseconds  = interrupt_timeout_.total_milliseconds();
            ar & BOOST_SERIALIZATION_NVP(interrupt_timeout_milliseconds);
            std::string hostname = youtils::System::getHostName();
            uint64_t pid = youtils::System::getPID();
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

BOOST_CLASS_VERSION(backend::Lock, 0);

#endif // RESTLOCK_H_


// Local Variables: **
// mode: c++ **
// End: **
