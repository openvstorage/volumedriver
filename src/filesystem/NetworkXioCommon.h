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

#ifndef __NETWORK_XIO_COMMON_H_
#define __NETWORK_XIO_COMMON_H_

#include <sys/eventfd.h>

#define ATTR_UNUSED     __attribute__((unused))

namespace volumedriverfs
{

struct EventFD
{
    EventFD()
    {
        evfd_ = eventfd(0, EFD_NONBLOCK);
        if (evfd_ < 0)
        {
            throw std::runtime_error("failed to create eventfd");
        }
    }

    ~EventFD()
    {
        if (evfd_ != -1)
        {
            close(evfd_);
        }
    }
    operator int() const { return evfd_; }
private:
    int evfd_;
};

static inline int
xeventfd_read(int fd)
{
    int ret;
    eventfd_t value = 0;
    do {
        ret = eventfd_read(fd, &value);
    } while (ret < 0 && errno == EINTR);
    if (ret == 0)
    {
        ret = value;
    }
    else if (errno != EAGAIN)
    {
        abort();
    }
    return ret;
}

static inline int
xeventfd_write(int fd)
{
    uint64_t u = 1;
    int ret;
    do {
        ret = eventfd_write(fd, static_cast<eventfd_t>(u));
    } while (ret < 0 && (errno == EINTR || errno == EAGAIN));
    if (ret < 0)
    {
        abort();
    }
    return ret;
}

} //namespace

enum class NetworkXioMsgOpcode
{
    Noop,
    OpenReq,
    OpenRsp,
    CloseReq,
    CloseRsp,
    ReadReq,
    ReadRsp,
    WriteReq,
    WriteRsp,
    FlushReq,
    FlushRsp,
    ErrorRsp,
    ShutdownRsp,
    CreateVolumeReq,
    CreateVolumeRsp,
    RemoveVolumeReq,
    RemoveVolumeRsp,
    ListVolumesReq,
    ListVolumesRsp,
    StatVolumeReq,
    StatVolumeRsp,
    ListSnapshotsReq,
    ListSnapshotsRsp,
    CreateSnapshotReq,
    CreateSnapshotRsp,
    DeleteSnapshotReq,
    DeleteSnapshotRsp,
    RollbackSnapshotReq,
    RollbackSnapshotRsp,
    IsSnapshotSyncedReq,
    IsSnapshotSyncedRsp,
};

#endif //__NETWORK_XIO_COMMON_H_
