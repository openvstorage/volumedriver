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

#ifndef VD_FAILOVER_CACHE_MODE_H_
#define VD_FAILOVER_CACHE_MODE_H_

#include <iosfwd>
#include <cstdint>

namespace volumedriver
{

enum class FailOverCacheMode: uint8_t
{
    // We are using 0 in serialization to
    Asynchronous = 1,
    Synchronous = 2,
};

std::ostream&
operator<<(std::ostream&,
           const FailOverCacheMode a);

std::istream&
operator>>(std::istream&,
        FailOverCacheMode&);

}

#endif // !VD_FAILOVER_CACHE_MODE_H_
