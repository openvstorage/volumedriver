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

#ifndef VD_NSID_MAP_BUILDER_H_
#define VD_NSID_MAP_BUILDER_H_

#include "NSIDMap.h"
#include "SnapshotPersistor.h"

#include <youtils/Logging.h>

#include <backend/BackendInterface.h>

namespace volumedriver
{

class NSIDMapBuilder
{
public:
    explicit NSIDMapBuilder(NSIDMap& nsid)
        : nsid_map_(nsid)
    {}

    ~NSIDMapBuilder() = default;

    NSIDMapBuilder(const NSIDMapBuilder&) = delete;

    NSIDMapBuilder&
    operator=(const NSIDMapBuilder&) = delete;

    void
    operator()(const SnapshotPersistor&,
               BackendInterfacePtr& bi,
               const std::string& /* snapshot_name */,
               SCOCloneID clone_id)
    {
        nsid_map_.set(clone_id,
                      bi->clone());
    }

    static const FromOldest direction = FromOldest::T;

private:
    DECLARE_LOGGER("NSIDMapBuilder");

    NSIDMap& nsid_map_;
};

}

#endif // !VD_NSIDMAP_BUILDER_H_
