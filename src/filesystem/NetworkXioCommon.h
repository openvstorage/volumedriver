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

#define get_offset(type, member) ((size_t)(&((type*)(1))->member)-1)
#define get_container_of(ptr, type, member) ((type *)((char *)(ptr) - \
                        get_offset(type, member)))

#define ATTR_UNUSED     __attribute__((unused))

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
};

#endif //__NETWORK_XIO_COMMON_H_
