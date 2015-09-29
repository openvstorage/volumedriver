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

#include "EntryToolCut.h"

namespace toolcut
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

ClusterLocationToolCut
EntryToolCut::clusterLocation() const
{
    return ClusterLocationToolCut(entry_->clusterLocation());
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

// Local Variables: **
// mode: c++ **
// End: **
