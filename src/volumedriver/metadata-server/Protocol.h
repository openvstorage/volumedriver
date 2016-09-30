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

#ifndef META_DATA_SERVER_MESSAGE_HEADERS_H_
#define META_DATA_SERVER_MESSAGE_HEADERS_H_

#include <youtils/Assert.h>
#include <youtils/SharedMemoryRegionId.h>

PRAGMA_IGNORE_WARNING_BEGIN("-Wnon-virtual-dtor");
#include "metadata-server/Protocol.capnp.h"
PRAGMA_IGNORE_WARNING_END;
#include <cstdint>
#include <iosfwd>

#include <youtils/IOException.h>
#include <youtils/OurStrongTypedef.h>

OUR_STRONG_NON_ARITHMETIC_TYPEDEF(uint64_t, Magic, metadata_server_protocol);
OUR_STRONG_NON_ARITHMETIC_TYPEDEF(uint64_t, Tag, metadata_server_protocol);

namespace metadata_server_protocol
{

constexpr uint64_t magic = 0xb0a710ad;

MAKE_EXCEPTION(Exception, fungi::IOException);
MAKE_EXCEPTION(NoMagicException, Exception);

// * messages consist of 2 parts:
// ** header (at the moment a struct of fixed size): provides enough
//    information to decode the body.
//    XXX: Use the old chum Cap'n P. for headers as well? Then again, if we want to go to
//    ZMQ it's better not having to decode when routing.
// ** variable size, uses capnproto (boost::protobuf doesn't work well with blobs)
// * simple request / response model with the option of enhancing it later with queueing:
// ** client sends request that indicates the desired operation + input data,
// ** server responds with status + output data
// * responses need to carry the tag of the associated request
// * fastpath: requests and responses have `memory_region' + `offset' members.
//   if non-null this indicates that data is not transferred in the message body but via
//   shared memory.

struct RequestHeader
{
    enum class Type
        : uint32_t
    {
        Drop = 0,
        Clear = 1,
        List = 2,
        MultiGet = 3,
        MultiSet = 4,
        SetRole = 5,
        GetRole = 6,
        Open = 7,
        Ping = 8,
        ApplyRelocationLogs = 9,
        CatchUp = 10,
        GetTableCounters = 11,
        GetOwnerTag = 12,
    };

    RequestHeader() = default;

    RequestHeader(Type r,
                  uint32_t s,
                  Tag t,
                  youtils::SharedMemoryRegionId omr = youtils::SharedMemoryRegionId(0),
                  uint64_t ooff = 0,
                  youtils::SharedMemoryRegionId imr = youtils::SharedMemoryRegionId(0),
                  uint64_t ioff = 0)
        : request_type(r)
        , size(s)
        , tag(t)
        , out_region(omr)
        , out_offset(ooff)
        , in_region(imr)
        , in_offset(ioff)
    {}

    ~RequestHeader() = default;

    RequestHeader(const RequestHeader&) = default;

    RequestHeader&
    operator=(const RequestHeader&) = default;

    Magic magic = Magic(::metadata_server_protocol::magic);
    Type request_type = Type::GetRole;
    uint32_t pad_ = 0;
    uint64_t size = 0;
    Tag tag = Tag(0);
    youtils::SharedMemoryRegionId out_region = youtils::SharedMemoryRegionId(0);
    uint64_t out_offset = 0;
    youtils::SharedMemoryRegionId in_region = youtils::SharedMemoryRegionId(0);
    uint64_t in_offset = 0;
};

static_assert(sizeof(RequestHeader) == 64,
              "unexpected sizeof(Request)");

std::ostream&
operator<<(std::ostream&,
           RequestHeader::Type);

struct ResponseHeader
{
    enum class Type
        : uint32_t
    {
        Ok = 1000,
        UnknownRequest = 1001,
        ProtocolError = 1002,
        Error = 1003,
    };

    enum Flags
    {
        UseShmem = 1U << 0,
    };

    ResponseHeader() = default;

    ResponseHeader(Type r,
                   uint32_t s,
                   Tag t,
                   uint32_t f = 0)
        : response_type(r)
        , flags(f)
        , size(s)
        , tag(t)
    {}

    ~ResponseHeader() = default;

    ResponseHeader(const ResponseHeader&) = default;

    ResponseHeader&
    operator=(const ResponseHeader&) = default;

    Magic magic = Magic(::metadata_server_protocol::magic);
    Type response_type = Type::Error;
    uint32_t flags = 0;
    uint64_t size = 0;
    Tag tag = static_cast<Tag>(-1);
};

static_assert(sizeof(ResponseHeader) == 32,
              "unexpected sizeof(Response)");

std::ostream&
operator<<(std::ostream&,
           ResponseHeader::Type);

template<enum RequestHeader::Type>
struct RequestTraits
{};

#define MAKE_REQUEST(req)                                               \
                                                                        \
    template<>                                                          \
    struct RequestTraits<RequestHeader::Type :: req>                    \
    {                                                                   \
        using Params = Methods:: req ## Params;                         \
        using Results = Methods:: req ## Results;                       \
        static constexpr RequestHeader::Type request_type = RequestHeader::Type:: req; \
    }

MAKE_REQUEST(Drop);
MAKE_REQUEST(Clear);
MAKE_REQUEST(List);
MAKE_REQUEST(MultiGet);
MAKE_REQUEST(MultiSet);
MAKE_REQUEST(SetRole);
MAKE_REQUEST(GetRole);
MAKE_REQUEST(Open);
MAKE_REQUEST(Ping);
MAKE_REQUEST(ApplyRelocationLogs);
MAKE_REQUEST(CatchUp);
MAKE_REQUEST(GetTableCounters);
MAKE_REQUEST(GetOwnerTag);

#undef MAKE_REQUEST

}

#endif // !META_DATA_SERVER_MESSAGE_HEADERS_H_
