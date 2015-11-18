// Copyright 2015 iNuron NV
//
// licensed under the Apache license, Version 2.0 (the "license");
// you may not use this file except in compliance with the license.
// You may obtain a copy of the license at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the license is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the license for the specific language governing permissions and
// limitations under the license.

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
