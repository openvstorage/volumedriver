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
