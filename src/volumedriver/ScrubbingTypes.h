// Copyright 2015 iNuron NV
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
