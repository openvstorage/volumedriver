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

#include "ScrubReply.h"

#include <iostream>
#include <sstream>

namespace scrubbing
{

namespace
{

using IArchive = boost::archive::xml_iarchive;
using OArchive = boost::archive::xml_oarchive;

const char archive_name[] = "scrubreply";

}

ScrubReply::ScrubReply(const std::string& s)
{
    std::istringstream is(s);
    IArchive ia(is);
    ia & boost::serialization::make_nvp(archive_name,
                                        *this);
}

std::string
ScrubReply::str() const
{
    std::ostringstream os;
    OArchive oa(os);
    oa & boost::serialization::make_nvp(archive_name,
                                        *this);
    return os.str();
}

bool
ScrubReply::operator==(const ScrubReply& other) const
{
    return
        ns_ == other.ns_ and
        snapshot_name_ == other.snapshot_name_ and
        scrub_result_name_ == other.scrub_result_name_;
}

bool
ScrubReply::operator!=(const ScrubReply& other) const
{
    return not operator==(other);
}

bool
ScrubReply::operator<(const ScrubReply& other) const
{
    if (ns_ < other.ns_)
    {
        return true;
    }
    else if (ns_ == other.ns_)
    {
        if (snapshot_name_  < other.snapshot_name_)
        {
            return true;
        }
        else if (snapshot_name_ == other.snapshot_name_)
        {
            return scrub_result_name_ < other.scrub_result_name_;
        }
    }

    return false;
}

std::ostream&
operator<<(std::ostream& os,
           const ScrubReply& rep)
{
    return os << "ScrubReply(ns=" << rep.ns_ <<
        ", snap=" << rep.snapshot_name_ <<
        ", res=" << rep.scrub_result_name_ << ")";
}

}
