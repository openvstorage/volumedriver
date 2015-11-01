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

#ifndef VFS_FILESYSTEM_EVENTS_H_
#define VFS_FILESYSTEM_EVENTS_H_

#include "FileSystemEvents.pb.h"
#include "FrontendPath.h"
#include "NodeId.h"

#include <youtils/Logging.h>

#include <volumedriver/Types.h>

namespace volumedriverfs
{

struct FileSystemEvents
{
    DECLARE_LOGGER("FileSystemEvents");

    static events::Event
    volume_create(const volumedriver::VolumeId& id,
                  const FrontendPath& path,
                  uint64_t size);

    static events::Event
    volume_create_failed(const FrontendPath& path,
                         const std::string& reason);

    static events::Event
    volume_delete(const volumedriver::VolumeId& id,
                  const FrontendPath& path);

    static events::Event
    volume_resize(const volumedriver::VolumeId& id,
                  const FrontendPath& path,
                  uint64_t size);

    static events::Event
    volume_rename(const volumedriver::VolumeId& id,
                  const FrontendPath& from,
                  const FrontendPath& to);

    static events::Event
    file_create(const FrontendPath& p);

    static events::Event
    file_delete(const FrontendPath& p);

    static events::Event
    file_write(const FrontendPath& p);

    static events::Event
    file_rename(const FrontendPath& from,
                const FrontendPath& to);

    static events::Event
    up_and_running(const std::string& mntpoint);

    static events::Event
    redirect_timeout_while_online(const NodeId& remote_node_id);
};

}

#endif // !VFS_FILESYSTEM_EVENTS_H_
