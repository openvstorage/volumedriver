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
