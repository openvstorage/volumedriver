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

#include "MessageUtils.h"

namespace vfsprotocol
{

namespace vfs = volumedriverfs;

PingMessage
MessageUtils::create_ping_message(const vfs::NodeId& sender_id)
{
    PingMessage msg;
    msg.set_sender_id(sender_id.str());

    msg.CheckInitialized();
    return msg;
}

ReadRequest
MessageUtils::create_read_request(const vfs::Object& obj,
                                  const uint64_t size,
                                  const uint64_t offset)
{
    ReadRequest msg;
    msg.set_object_id(obj.id.str());
    msg.set_object_type(static_cast<uint32_t>(obj.type));

    msg.set_size(size);
    msg.set_offset(offset);

    msg.CheckInitialized();

    return msg;
}

WriteRequest
MessageUtils::create_write_request(const vfs::Object& obj,
                                   const uint64_t size,
                                   const uint64_t offset)
{
    WriteRequest msg;
    msg.set_object_id(obj.id.str());
    msg.set_object_type(static_cast<uint32_t>(obj.type));

    msg.set_size(size);
    msg.set_offset(offset);

    msg.CheckInitialized();

    return msg;
}

WriteResponse
MessageUtils::create_write_response(const uint64_t size)
{
    WriteResponse msg;
    msg.set_size(size);

    msg.CheckInitialized();

    return msg;
}

SyncRequest
MessageUtils::create_sync_request(const vfs::Object& obj)
{
    SyncRequest msg;
    msg.set_object_id(obj.id.str());
    msg.set_object_type(static_cast<uint32_t>(obj.type));

    msg.CheckInitialized();

    return msg;
}

ResizeRequest
MessageUtils::create_resize_request(const vfs::Object& obj,
                                    uint64_t newsize)
{
    ResizeRequest msg;
    msg.set_object_id(obj.id.str());
    msg.set_object_type(static_cast<uint32_t>(obj.type));

    msg.set_size(newsize);

    msg.CheckInitialized();

    return msg;
}

GetSizeRequest
MessageUtils::create_get_size_request(const vfs::Object& obj)
{
    GetSizeRequest msg;
    msg.set_object_id(obj.id.str());
    msg.set_object_type(static_cast<uint32_t>(obj.type));

    msg.CheckInitialized();

    return msg;
}

GetSizeResponse
MessageUtils::create_get_size_response(const uint64_t size)
{
    GetSizeResponse msg;
    msg.set_size(size);

    msg.CheckInitialized();

    return msg;
}

DeleteRequest
MessageUtils::create_delete_request(const vfs::Object& obj)
{
    DeleteRequest msg;
    msg.set_object_id(obj.id.str());
    msg.set_object_type(static_cast<uint32_t>(obj.type));

    msg.CheckInitialized();

    return msg;
}

TransferRequest
MessageUtils::create_transfer_request(const vfs::Object& obj,
                                      const vfs::NodeId& target_node_id,
                                      const boost::chrono::milliseconds& sync_timeout_ms)
{
    TransferRequest msg;
    msg.set_object_id(obj.id.str());
    msg.set_object_type(static_cast<uint32_t>(obj.type));
    msg.set_target_node_id(target_node_id.str());
    msg.set_sync_timeout_ms(sync_timeout_ms.count());

    msg.CheckInitialized();

    return msg;
}

}
