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

#ifndef VD_FAILOVER_CACHE_CLIENTINTERFACE_H
#define VD_FAILOVER_CACHE_CLIENTINTERFACE_H

#include "FailOverCacheBridgeCommon.h"
#include "FailOverCacheMode.h"
#include "failovercache/fungilib/Mutex.h"

#include "SCOProcessorInterface.h"

namespace volumedriver
{

class FailOverCacheClientInterface
{
public:

    virtual ~FailOverCacheClientInterface() = default;

    virtual void
    initialize(Volume*) = 0;

    virtual const char*
    getName() const = 0;

    virtual void
    destroy(SyncFailOverToBackend) = 0;

    virtual bool
    addEntries(const std::vector<ClusterLocation>& locs,
               size_t num_locs,
               uint64_t start_address,
               const uint8_t* data) = 0;

    virtual bool
    backup() = 0;

    virtual void
    newCache(std::unique_ptr<FailOverCacheProxy>) = 0;

    virtual uint32_t
    getDefaultRequestTimeout()
    {
        return 60; // seconds
    }

    virtual void
    setRequestTimeout(const uint32_t seconds) = 0;

    virtual void
    removeUpTo(const SCO& sconame) = 0;

    virtual void
    Flush() = 0;

    virtual void
    Clear() = 0;

    virtual uint64_t
    getSCOFromFailOver(SCO sconame,
                       SCOProcessorFun processor) = 0;

    virtual FailOverCacheMode
    mode() const = 0;

    virtual fungi::Mutex&
    getMutex() = 0;

    virtual std::unique_ptr<FailOverCacheProxy>&
    getCache() = 0;

};

} // namespace volumedriver

#endif // VD_FAILOVER_CACHE_CLIENTINTERFACE_H
