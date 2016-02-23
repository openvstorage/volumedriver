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
