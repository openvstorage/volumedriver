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

#ifndef VFS_FILESYSTEM_EVENTS_H_
#define VFS_FILESYSTEM_EVENTS_H_

#include "FileSystemEvents.pb.h"
#include "FrontendPath.h"
#include "NodeId.h"

#include <youtils/Logging.h>

#include <volumedriver/Types.h>

namespace volumedriverfs
{

class ObjectId;

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

    static events::Event
    owner_changed(const ObjectId&,
                  const NodeId&);
};

}

#endif // !VFS_FILESYSTEM_EVENTS_H_
