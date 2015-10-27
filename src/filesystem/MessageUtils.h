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

#ifndef VFS_MESSAGE_UTILS_H_
#define VFS_MESSAGE_UTILS_H_

#include "Messages.pb.h"
#include "NodeId.h"
#include "Object.h"

#include <youtils/Logging.h>

#include <volumedriver/Types.h>

namespace vfsprotocol
{

struct MessageUtils
{
    static PingMessage
    create_ping_message(const volumedriverfs::NodeId& sender_id);

    static ReadRequest
    create_read_request(const volumedriverfs::Object& obj,
                        const uint64_t size,
                        const uint64_t offset);

    static WriteRequest
    create_write_request(const volumedriverfs::Object& obj,
                         const uint64_t size,
                         const uint64_t offset);

    static WriteResponse
    create_write_response(const uint64_t size);

    static SyncRequest
    create_sync_request(const volumedriverfs::Object& obj);

    static GetSizeRequest
    create_get_size_request(const volumedriverfs::Object& obj);

    static GetSizeResponse
    create_get_size_response(const uint64_t size);

    static ResizeRequest
    create_resize_request(const volumedriverfs::Object& obj,
                          uint64_t newsize);

    static DeleteRequest
    create_delete_request(const volumedriverfs::Object& obj);

    static TransferRequest
    create_transfer_request(const volumedriverfs::Object& obj,
                            const volumedriverfs::NodeId& target_node_id,
                            const boost::chrono::milliseconds& sync_timeout_ms);

};

}

#endif //!VFS_MESSAGE_UTILS_H_
