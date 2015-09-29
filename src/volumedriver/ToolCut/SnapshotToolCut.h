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

#ifndef SNAPSHOT_TOOLCUT_H
#define SNAPSHOT_TOOLCUT_H

#include <boost/python/list.hpp>
#include <volumedriver/Snapshot.h>

namespace toolcut
{
namespace vd = volumedriver;
namespace bpy = boost::python;

// typically the stuff from TLogs should be here too
class SnapshotToolCut
{
    class Iter;
    friend class Iter;

public:
    explicit SnapshotToolCut(const vd::Snapshot& snapshot);

    vd::SnapshotNum
    getNum() const;

    std::string
    name() const;

    // boost::python::list
    // getTLogs() const;

    void
    forEach(const boost::python::object& obj) const;

    uint64_t
    stored() const;

    const std::string
    getUUID() const;

    bool
    hasUUIDSpecified() const;

    const std::string
    getDate() const;

    std::string
    str() const;

    std::string
    repr() const;

    bool
    inBackend() const;

private:
    const vd::Snapshot snapshot_;
};
}

#endif // SNAPSHOT_TOOLCUT_H


// Local Variables: **
// compile-command: "scons --kernel_version=system -D -j 4" **
// End: **
