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
