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

#include "ClusterLocation.h"

#include <boost/io/ios_state.hpp>

namespace volumedriver
{

ClusterLocation::ClusterLocation(const std::string& in)
    : sco_(in.substr(0, 14))
{
    if(not isClusterLocationString(in, false))
    {
        throw fungi::IOException("Not an offset string");
    }

    offset_ = SCO::gethexnum(in, 15, 19);
}

std::string
ClusterLocation::str() const
{
    std::stringstream ss;
    ss << *this;
    return ss.str();
}

bool
ClusterLocation::isClusterLocationString(const std::string& str,
                                         bool test_sco_string)
{
    if (str.length() != 19)
    {
        return false;
    }

    if (test_sco_string)
    {
        const std::string scostr(str.substr(0, 14));
        if (not SCO::isSCOString(scostr))
        {
            return false;
        }
    }

    if (str[14] != ':')
    {
        return false;
    }

    return SCO::hasonlyhexdigits(str, 15, 19);
}

std::ostream&
operator<<(std::ostream& ostr,
           const volumedriver::ClusterLocation& loc)
{
    boost::io::ios_all_saver osg(ostr);

    return ostr << loc.sco().str()
                << ":" << std::hex  << std::setfill('0') << std::setw(4)
                << loc.offset();
}

fungi::IOBaseStream&
operator<<(fungi::IOBaseStream& ostream,
           const ClusterLocation& loc)
{
    return ostream << loc.offset_ << loc.sco_;
}

fungi::IOBaseStream&
operator>>(fungi::IOBaseStream& ostream,
           ClusterLocation& loc)
{
    uint16_t offset;

    ostream >> offset;
    loc.offset_ = offset;
    return ostream >> loc.sco_;
}

static_assert(sizeof(ClusterLocation) == 8, "Wrong size for ClusterLocation");

}

// Local Variables: **
// End: **
