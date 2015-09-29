// Copyright 2015 Open vStorage NV
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

const char*
request_type_to_string(const RequestType t);

const char*
response_type_to_string(const ResponseType t);

}

#endif // !VFS_PROTOCOL_H_
