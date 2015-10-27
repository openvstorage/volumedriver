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
