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

#include "MessageUtils.h"

namespace vfsprotocol
{

namespace vd = volumedriver;
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
MessageUtils::create_write_response(const uint64_t size,
                                    const vd::DtlInSync dtl_in_sync)
{
    WriteResponse msg;
    msg.set_size(size);
    msg.set_dtl_in_sync(dtl_in_sync == vd::DtlInSync::T);

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

SyncResponse
MessageUtils::create_sync_response(const vd::DtlInSync dtl_in_sync)
{
    SyncResponse msg;
    msg.set_dtl_in_sync(dtl_in_sync == vd::DtlInSync::T);

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

GetClusterMultiplierRequest
MessageUtils::create_get_cluster_multiplier_request(const vfs::Object& obj)
{
    GetClusterMultiplierRequest msg;
    msg.set_object_id(obj.id.str());
    msg.set_object_type(static_cast<uint32_t>(obj.type));

    msg.CheckInitialized();

    return msg;
}

GetClusterMultiplierResponse
MessageUtils::create_get_cluster_multiplier_response(const vd::ClusterMultiplier cm)
{
    GetClusterMultiplierResponse msg;
    msg.set_size(static_cast<uint32_t>(cm));

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
