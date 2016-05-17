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

#include "ClusterLocationAndHash.h"

std::ostream&
operator<<(std::ostream& ostr,
           const volumedriver::ClusterLocationAndHash& loc)
{
#ifdef ENABLE_MD5_HASH
    return ostr << loc.clusterLocation
                << loc.weed_;
#else
    return ostr << loc.clusterLocation;
#endif
}

namespace volumedriver
{
#ifdef ENABLE_MD5_HASH
static_assert(sizeof(ClusterLocationAndHash) == 24, "Wrong size for ClusterLocationAndHash");
#else
static_assert(sizeof(ClusterLocationAndHash) == 8, "Wrong size for ClusterLocationAndHash");
#endif
}

// Local Variables: **
// End: **
