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

#ifndef VFS_PROTOCOL_H_
#define VFS_PROTOCOL_H_

#include "Messages.pb.h"

#include <boost/mpl/map.hpp>

#include <youtils/OurStrongTypedef.h>

OUR_STRONG_ARITHMETIC_TYPEDEF(uint64_t, Tag, vfsprotocol);

namespace vfsprotocol
{

// ZMQ message parts:
// (1) RequestType / ResponseCode - 4 bytes
// (2) Tag - 8 bytes. To be treated as opaque by the receiving side.
// (3) optional part(s) - google protobuf messages, raw read/write data ....

// Request- and ResponseTypes should not overlap. Lump them together in one enum?
enum class RequestType
: uint32_t
{
    Read = 1,
    Write = 2,
    Sync = 3,
    GetSize = 4,
    Resize = 5,
    Delete = 6,
    Transfer = 7,
    Ping = 8,
    GetClusterMultiplier = 9,
    GetCloneNamespaceMap = 10,
    GetPage = 11,
};

enum class ResponseType
: uint32_t
{
    Ok = 1001,
    UnknownRequest = 1002,
    ProtocolError = 1003,
    ObjectNotRunningHere = 1004,
    IOError = 1005,
    Timeout = 1006,
};

template<typename T>
struct RequestTraits
{};

// Tie the RequestType to an actual Message. We could go further and also
// add the expected (optional!) response message there (if ResponseType::Ok).
#define MAKE_REQUEST_TRAITS(Request, req_type)                  \
    template<>                                                  \
    struct RequestTraits<Request>                               \
    {                                                           \
        static constexpr RequestType request_type = req_type;   \
    }                                                           \

MAKE_REQUEST_TRAITS(PingMessage, RequestType::Ping);
MAKE_REQUEST_TRAITS(ReadRequest, RequestType::Read);
MAKE_REQUEST_TRAITS(WriteRequest, RequestType::Write);
MAKE_REQUEST_TRAITS(SyncRequest, RequestType::Sync);
MAKE_REQUEST_TRAITS(GetSizeRequest, RequestType::GetSize);
MAKE_REQUEST_TRAITS(ResizeRequest, RequestType::Resize);
MAKE_REQUEST_TRAITS(DeleteRequest, RequestType::Delete);
MAKE_REQUEST_TRAITS(TransferRequest, RequestType::Transfer);
MAKE_REQUEST_TRAITS(GetClusterMultiplierRequest, RequestType::GetClusterMultiplier);
MAKE_REQUEST_TRAITS(GetCloneNamespaceMapRequest, RequestType::GetCloneNamespaceMap);
MAKE_REQUEST_TRAITS(GetPageRequest, RequestType::GetPage);

const char*
request_type_to_string(const RequestType t);

const char*
response_type_to_string(const ResponseType t);

}

#endif // !VFS_PROTOCOL_H_
