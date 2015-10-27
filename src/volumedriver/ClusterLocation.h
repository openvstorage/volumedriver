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

#ifndef CLUSTERLOCATION_H_
#define CLUSTERLOCATION_H_

#include "SCO.h"
#include "Types.h"

#include <iosfwd>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/utility.hpp>

#include <youtils/Assert.h>
#include "failovercache/fungilib/IOBaseStream.h"

namespace volumedriver
{

class ClusterLocation;

std::ostream&
operator<<(std::ostream& ostr,
           const volumedriver::ClusterLocation& loc);

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& ostream,
           const ClusterLocation& loc);

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& ostream,
           ClusterLocation& loc);

class ClusterLocation
{
public:
    explicit ClusterLocation(SCONumber number,
                             SCOOffset offset = 0,
                             SCOCloneID cloneID = SCOCloneID(0),
                             SCOVersion version = SCOVersion(0))
        : offset_(offset)
        , sco_(number, cloneID, version)
    {}

    ClusterLocation(const SCO& sco,
                    SCOOffset offset)
        : offset_(offset)
        , sco_(sco)
    {}

    ClusterLocation()
        :offset_(0)
    {}


    explicit ClusterLocation(const std::string& str);

    static bool
    isClusterLocationString(const std::string& str,
                            const bool testSCOStringToo = true);

    std::string
    str() const;

    bool isNull() const
    {
        return sco_.number() == 0;
    }

    SCO
    sco() const
    {
        return sco_;
    }

    SCOOffset
    offset() const
    {
        return offset_;
    }

    SCOVersion
    version() const
    {
        return sco_.version();
    }

    SCOCloneID
    cloneID() const
    {
        return sco_.cloneID();
    }

    void
    offset(SCOOffset off)
    {
        offset_ = off;
    }

    void
    cloneID(SCOCloneID in)
    {
        sco_.cloneID(in);
    }

    SCONumber
    number() const
    {
        return sco_.number();
    }

    bool
    operator==(const ClusterLocation& inOther) const
    {
        return sco_ == inOther.sco_ and
            offset_ == inOther.offset_;
    }

    bool
    operator!=(const ClusterLocation& inOther) const
    {
        return !(*this == inOther);
    }

    template<class Archive>
    void serialize(Archive& ar, const unsigned int /*version*/)
    {
        ar & sco_;
        ar & offset_;
    }

    void
    incrementOffset(SCOOffset inc = 1)
    {
        offset_ += inc;
    }

    void
    incrementVersion(SCOVersion inc = SCOVersion(1))
    {
        sco_.incrementVersion(inc);
    }

    void
    decrementVersion(SCOVersion inc = SCOVersion(1))
    {
        sco_.decrementVersion(inc);
    }

    void
    incrementCloneID(SCOCloneID inc = SCOCloneID(1))
    {
        sco_.incrementCloneID(inc);
    }

    void
    decrementCloneID(SCOCloneID dec = SCOCloneID(1))
    {
        sco_.decrementCloneID(dec);
    }

    void
    incrementNumber(SCONumber inc = SCONumber(1))
    {
        sco_.incrementNumber(inc);
    }

    void
    decrementNumber(SCONumber dec = SCONumber(1))
    {
        sco_.decrementNumber(dec);
    }

private:
    DECLARE_LOGGER("ClusterLocation");

    friend fungi::IOBaseStream&
    operator<<(fungi::IOBaseStream& ostream,
               const ClusterLocation& loc);

    friend fungi::IOBaseStream&
    operator>>(fungi::IOBaseStream& ostream,
               ClusterLocation& loc);

    bool
    operator<(const ClusterLocation& inOther);

    bool
    operator<=(const ClusterLocation& inOther);

    bool
    operator>=(const ClusterLocation& inOther);

    bool
    operator>(const ClusterLocation& inOther);

    SCOOffset offset_;
    SCO sco_;

} __attribute__((__packed__));

}

BOOST_CLASS_VERSION(volumedriver::ClusterLocation, 0);

#endif // !CLUSTERLOCATION_H_

// Local Variables: **
// mode: c++ **
// End: **
