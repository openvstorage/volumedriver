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

#ifndef SCRUBBING_TYPES_H
#define SCRUBBING_TYPES_H

#include "Types.h"

#include <youtils/OurStrongTypedef.h>

namespace scrubbing
{

// log_2 of the number of clusters in a Region as a power
typedef uint32_t RegionExponent;

}

OUR_STRONG_ARITHMETIC_TYPEDEF(uint64_t, ClusterRegionSize, scrubbing);

// Which region
OUR_STRONG_ARITHMETIC_TYPEDEF(uint32_t, RegionIndex, scrubbing);

#endif // SCRUBBING_TYPES_H

// Local Variables: **
// End: **
