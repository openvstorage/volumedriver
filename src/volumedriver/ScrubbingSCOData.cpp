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

#include "ScrubbingSCOData.h"
#include "ClusterLocation.h"
namespace scrubbing
{

static_assert(sizeof(ScrubbingSCOData) == 12, "unexpected size for ScrubbingSCOData");

using namespace volumedriver;

ScrubbingSCOData::ScrubbingSCOData(SCO sconame)
        :sconame_(sconame),
         usageCount(0),
         size(1),
         state(State::Unknown)
    {};


std::ostream&
operator<<(std::ostream& out, const ScrubbingSCOData::State& state)
{
    switch(state)
    {
    case ScrubbingSCOData::State::Unknown:
        out << "Unknown";
        break;
    case ScrubbingSCOData::State::Scrubbed:
        out << "Scrubbed";
        break;
    case ScrubbingSCOData::State::NotScrubbed:
        out << "NotScrubbed";
        break;
    case ScrubbingSCOData::State::Reused:
        out << "Reused";
        break;
    }
    return out;
}


std::ostream&
operator<<(std::ostream& out, const ScrubbingSCODataVector& data)
{
    out << "SCOName        | Use| Ref| State" << std::endl;

    for(ScrubbingSCODataVector::const_iterator it = data.begin();
        it != data.end();
        ++it)
    {
        out << it->sconame_ << " | "
            << std::dec << std::setw(2) << it->usageCount << " | "
            << std::dec << std::setw(2) << it->size << " | "
            << it->state << std::endl;

    }
    return out;
}


}

// Local Variables: **
// mode : c++ **
// End: **
