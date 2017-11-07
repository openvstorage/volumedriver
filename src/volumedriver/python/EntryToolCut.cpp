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

#include "EntryToolCut.h"

namespace volumedriver
{

namespace python
{

volumedriver::Entry::Type
EntryToolCut::type() const
{
    return entry_->type();
}

volumedriver::CheckSum::value_type
EntryToolCut::getCheckSum() const
{
    return entry_->getCheckSum();
}

volumedriver::ClusterAddress
EntryToolCut::clusterAddress() const
{
    return entry_->clusterAddress();
}

volumedriver::ClusterLocation
EntryToolCut::clusterLocation() const
{
    return entry_->clusterLocation();
}

std::string
EntryToolCut::str() const
{
    std::stringstream ss;
    volumedriver::Entry::Type typ = entry_->type();
    ss << "type: " << typ << std::endl;
    switch(typ)
    {
    case volumedriver::Entry::Type::SyncTC:
        break;

    case volumedriver::Entry::Type::TLogCRC:
    case volumedriver::Entry::Type::SCOCRC:
        ss << "checksum: " << getCheckSum() << std::endl;
        break;
    case volumedriver::Entry::Type::LOC:
        ss << "clusterAddress: " << clusterAddress();
        ss << "clusterLocation: " << entry_->clusterLocation();
    }
    return ss.str();

}


std::string
EntryToolCut::repr() const
{
    return std::string("< Entry \n") + str() + "\n>";
}

}

}

// Local Variables: **
// mode: c++ **
// End: **
