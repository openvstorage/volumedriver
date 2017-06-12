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
