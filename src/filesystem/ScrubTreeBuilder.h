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

#ifndef VFS_SCRUB_TREE_BUILDER_H_
#define VFS_SCRUB_TREE_BUILDER_H_

#include "ScrubManager.h"

#include <youtils/Logging.h>

namespace volumedriver
{
class SnapshotName;
}

namespace scrubbing
{
class ScrubReply;
}

namespace volumedriverfs
{

class ObjectId;
class ClusterId;
class ObjectRegistry;

class ScrubTreeBuilder
{
public:
    explicit ScrubTreeBuilder(ObjectRegistry&);

    ~ScrubTreeBuilder() = default;

    ScrubTreeBuilder(const ScrubTreeBuilder&) = default;

    ScrubTreeBuilder&
    operator=(const ScrubTreeBuilder&) = default;

    ScrubTreeBuilder(ScrubTreeBuilder&&) = default;

    ScrubTreeBuilder&
    operator=(ScrubTreeBuilder&&) = default;

    ScrubManager::ClonePtrList
    operator()(const ObjectId&,
               const volumedriver::SnapshotName&);

private:
    DECLARE_LOGGER("ScrubTreeBuilder");

    ObjectRegistry& registry_;
};

}

#endif // !VFS_SCRUB_TREE_BUILDER_H_
