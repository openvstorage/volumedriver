// Copyright 2016 iNuron NV
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

#ifndef __NETWORK_XIO_COMMON_H_
#define __NETWORK_XIO_COMMON_H_

#include <sys/eventfd.h>

#define ATTR_UNUSED     __attribute__((unused))

namespace volumedriverfs
{

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
