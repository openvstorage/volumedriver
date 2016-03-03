// Copyright 2016 iNuron NV
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

#include "FailOverCacheClientInterface.h"
#include "FailOverCacheAsyncBridge.h"
#include "FailOverCacheSyncBridge.h"

namespace volumedriver
{

using Ptr = std::unique_ptr<FailOverCacheClientInterface>;

Ptr
FailOverCacheClientInterface::create(const FailOverCacheMode mode,
                                     const LBASize lba_size,
                                     const ClusterMultiplier cluster_multiplier,
                                     const size_t max_entries,
                                     const std::atomic<unsigned>& write_trigger)
{
    switch (mode)
    {
    case FailOverCacheMode::Asynchronous:
        return Ptr(new FailOverCacheAsyncBridge(lba_size,
                                                cluster_multiplier,
                                                max_entries,
                                                write_trigger));
    case FailOverCacheMode::Synchronous:
        return Ptr(new FailOverCacheSyncBridge(max_entries));
    }
}

} // namespace volumedriver
