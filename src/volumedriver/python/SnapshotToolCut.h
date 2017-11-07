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

#ifndef SNAPSHOT_TOOLCUT_H
#define SNAPSHOT_TOOLCUT_H

#include <boost/python/list.hpp>
#include <volumedriver/Snapshot.h>

namespace volumedriver
{

namespace python
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

}
#endif // SNAPSHOT_TOOLCUT_H


// Local Variables: **
// compile-command: "scons --kernel_version=system -D -j 4" **
// End: **
