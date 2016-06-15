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

#include "SnapshotToolCut.h"
#include "TLogToolCut.h"

namespace toolcut
{

SnapshotToolCut::SnapshotToolCut(const vd::Snapshot& snapshot)
    :snapshot_(snapshot)
{}

vd::SnapshotNum
SnapshotToolCut::getNum() const
{
    return snapshot_.snapshotNumber();
}

std::string
SnapshotToolCut::name() const
{
    return snapshot_.getName();
}


void
SnapshotToolCut::forEach(const boost::python::object& obj) const
{
    snapshot_.for_each_tlog([&](const vd::TLog& tlog)
                            {
                                obj(TLogToolCut(tlog));
                            });
}

uint64_t
SnapshotToolCut::stored() const
{
    return snapshot_.backend_size();
}
const std::string
SnapshotToolCut::getUUID() const
{
    return snapshot_.getUUID().str();
}


bool
SnapshotToolCut::hasUUIDSpecified() const
{
    return snapshot_.hasUUIDSpecified();
}

const std::string
SnapshotToolCut::getDate() const
{
    return snapshot_.getDate();
}

std::string
SnapshotToolCut::str() const
{
    std::stringstream ss;

    ss << "name: " << name() << std::endl
       << "backendSize: " << stored() << std::endl
       << "uuid: " << getUUID() << std::endl
       << "date: " << getDate();
    return ss.str();
}

std::string
SnapshotToolCut::repr() const
{
    return std::string("< Snapshot \n") + str() + "\n>";
}

bool
SnapshotToolCut::inBackend() const
{
    return snapshot_.inBackend();
}

}

// Local Variables: **
// mode: c++ **
// End: **
