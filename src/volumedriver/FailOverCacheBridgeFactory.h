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

#ifndef VD_FAILOVER_CACHE_BRIDGE_FACTORY_H
#define VD_FAILOVER_CACHE_BRIDGE_FACTORY_H

#include "FailOverCacheClientInterface.h"
#include "FailOverCacheMode.h"

namespace volumedriver
{

class FailOverCacheBridgeFactory
{
    FailOverCacheBridgeFactory() = delete; // no instances

public:

    static FailOverCacheClientInterface*
    create(FailOverCacheMode mode,
           const std::atomic<unsigned>& max_entries,
           const std::atomic<unsigned>& write_trigger);
};

} // namespace volumedriver

#endif // VD_FAILOVER_CACHE_BRIDGE_FACTORY_H
