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
    // - client sends one of the request opcodes (!= Ok / != NotOk)
    // - server side responds either with
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
};

std::ostream&
operator<<(std::ostream&,
           FailOverCacheCommand);

}

#endif // !VD_FAILOVERCACHE_COMMAND_H_
