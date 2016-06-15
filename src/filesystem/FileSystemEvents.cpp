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

#include "FileSystemEvents.h"
#include "Object.h"

namespace volumedriverfs
{

events::Event
FileSystemEvents::volume_create(const volumedriver::VolumeId& id,
                                const FrontendPath& path,
                                uint64_t size)
{
    events::Event ev;
    auto msg = ev.MutableExtension(events::volume_create);

    msg->set_name(id.str());
    msg->set_size(size);
    msg->set_path(path.string());

    return ev;
}

events::Event
FileSystemEvents::volume_create_failed(const FrontendPath& path,
                                       const std::string& reason)
{
    events::Event ev;
    auto msg = ev.MutableExtension(events::volume_create_failed);

    msg->set_path(path.string());
    msg->set_reason(reason);

    return ev;
}

events::Event
FileSystemEvents::volume_delete(const volumedriver::VolumeId& id,
                                const FrontendPath& path)
{
    events::Event ev;
    auto msg = ev.MutableExtension(events::volume_delete);

    msg->set_path(path.string());
    msg->set_name(id.str());

    return ev;
}

events::Event
FileSystemEvents::volume_resize(const volumedriver::VolumeId& id,
                                const FrontendPath& path,
                                uint64_t size)
{
    events::Event ev;
    auto msg = ev.MutableExtension(events::volume_resize);

    msg->set_path(path.string());
    msg->set_name(id.str());
    msg->set_size(size);

    return ev;
}

events::Event
FileSystemEvents::volume_rename(const volumedriver::VolumeId& id,
                                const FrontendPath& from,
                                const FrontendPath& to)
{
    events::Event ev;
    auto msg = ev.MutableExtension(events::volume_rename);

    msg->set_old_path(from.string());
    msg->set_new_path(to.string());
    msg->set_name(id.str());

    return ev;
}

namespace
{
template<typename T>
events::Event
file_event(const FrontendPath& p,
           const T& t)
{
    events::Event ev;

    auto msg = ev.MutableExtension(t);
    msg->set_path(p.string());

    return ev;
}

}

events::Event
FileSystemEvents::file_create(const FrontendPath& p)
{
    return file_event(p, events::file_create);
}

events::Event
FileSystemEvents::file_delete(const FrontendPath& p)
{
    return file_event(p, events::file_delete);
}

events::Event
FileSystemEvents::file_write(const FrontendPath& p)
{
    return file_event(p, events::file_write);
}

events::Event
FileSystemEvents::file_rename(const FrontendPath& from,
                              const FrontendPath& to)
{
    events::Event ev;
    auto msg = ev.MutableExtension(events::file_rename);

    msg->set_old_path(from.string());
    msg->set_new_path(to.string());

    return ev;
}

events::Event
FileSystemEvents::up_and_running(const std::string& mntpoint)
{
    events::Event ev;
    auto msg = ev.MutableExtension(events::up_and_running);

    msg->set_mountpoint(mntpoint);

    return ev;
}

events::Event
FileSystemEvents::redirect_timeout_while_online(const NodeId& remote_node_id)
{
    events::Event ev;
    auto msg = ev.MutableExtension(events::redirect_timeout_while_online);

    msg->set_remote_node_id(remote_node_id.str());

    return ev;
}

events::Event
FileSystemEvents::owner_changed(const ObjectId& oid,
                                const NodeId& new_owner_id)
{
    events::Event ev;
    auto msg = ev.MutableExtension(events::owner_changed);

    msg->set_name(oid.str());
    msg->set_new_owner_id(new_owner_id.str());

    return ev;
}

}
