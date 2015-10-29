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

#ifndef VD_FAILOVER_CACHE_BRIDGE_COMMON_H
#define VD_FAILOVER_CACHE_BRIDGE_COMMON_H

#include "ClusterLocation.h"
#include "FailOverCacheProxy.h"
#include "Types.h"

namespace volumedriver
{

BOOLEAN_ENUM(SyncFailOverToBackend);

// Z42: we want an exception hierarchy here.
class FailOverCacheNotConfiguredException
    : public fungi::IOException
{
public:
    FailOverCacheNotConfiguredException()
        : fungi::IOException("FailOverCache not configured")
    {}
};

} // namespace volumedriver

#endif // VD_FAILOVER_CACHE_BRIDGE_COMMON_H
