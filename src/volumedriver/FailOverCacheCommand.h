// Copyright (C) 2017 iNuron NV
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

#ifndef VD_FAILOVERCACHE_COMMAND_H_
#define VD_FAILOVERCACHE_COMMAND_H_

#include <cstdint>
#include <iosfwd>

namespace volumedriver
{

enum class FailOverCacheCommand
    : uint32_t
{
    // The old protocol, reverse engineered from the code:
    // - client sends a cork size (in case of TCP but not in case of RSocket),
    //   followed by one of the request opcodes (!= Ok / != NotOk), followed by
    //   request specific data
    // - server side responds either with (corks intermixed for TCP but not
    //   RSocket, once more)
    //   - Ok (successful RemoveUpTo, Register, AddEntries, Flush, Unregister,
    //     Clear)
    //   - NotOk (unsuccessful Register, Unregister, RemoveUpTo)
    //   - NotOk + connection termination (all unsuccessful requests other than
    //     the ones mentioned above)
    //   - Response data:
    //     - sequence of FailOverCacheEntry's terminated by ClusterLocation(0)
    //       in response to GetSCO and GetEntries
    //     - a pair of SCO names in response to GetSCORange
    // .
    // The serialization of the data is based on fungilib and is geared towards
    // streaming (strings or containers are prefixed with the number of chars or
    // items, for example).
    RemoveUpTo     =  0x1,
    GetSCO         =  0x2,
    Register       =  0x3,
    AddEntries     =  0x4,
    GetEntries     =  0x5,
    Flush          =  0x6,
    Ok             =  0x7,
    NotOk          =  0x8,
    Unregister     =  0x9,
    Clear          =  0xA,
    GetSCORange    =  0xB,
    //    Bye            =  0xff,
    //
    // The above is too adhoc and streaming is not a good match, so with the
    // introduction of async I/O the above is phased out / only kept for back-
    // ward compatibility and the protocol is revised
    // (ProtocolFeature::TunnelCapnProto):
    // - client sends TunnelCapnProtoHeader with opcode 'TunnelCapnProto'
    //   followed by a message serialized with Capn'Proto, optionally followed
    //   by raw bulk data
    // - server sends TunnelCapnProtoHeader with opcode 'TunnelCapnProto'
    //   followed message serialized with Capn'Proto, optionally followed by raw
    //   bulk data
    TunnelCapnProto = 0xF0C,
};

// See above: only applicable to ProtocolFeature::TunnelCapnProto. The first
// two fields are there for backward compatibility with the old protocol.
struct TunnelCapnProtoHeader
{
    const uint32_t magic_ = sizeof(opcode) + sizeof(tag) + sizeof(capnp_size) + sizeof(data_size);
    const FailOverCacheCommand opcode = FailOverCacheCommand::TunnelCapnProto;
    uint64_t tag;
    uint32_t capnp_size = 0;
    uint32_t data_size = 0;

    explicit TunnelCapnProtoHeader(uint64_t t = 0,
                                   uint32_t cs = 0,
                                   uint32_t ds = 0)
        : tag(t)
        , capnp_size(cs)
        , data_size(ds)
    {}
};

std::ostream&
operator<<(std::ostream&,
           FailOverCacheCommand);

}

#endif // !VD_FAILOVERCACHE_COMMAND_H_
