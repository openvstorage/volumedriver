// Copyright 2015 Open vStorage NV
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

#ifndef VD_FAILOVER_CACHE_SYNCBRIDGE_H
#define VD_FAILOVER_CACHE_SYNCBRIDGE_H

#include "FailOverCacheBridgeCommon.h"
#include "FailOverCacheStreamers.h"
#include "FailOverCacheProxy.h"
#include "FailOverCacheClientInterface.h"
#include "SCO.h"

#include "failovercache/fungilib/CondVar.h"
#include "failovercache/fungilib/Runnable.h"
#include "failovercache/fungilib/Thread.h"

#include <youtils/FileDescriptor.h>
#include <youtils/IOException.h>

namespace volumedriver
{

class FailOverCacheSyncBridge
    : public FailOverCacheClientInterface
{

public:
    FailOverCacheSyncBridge();

    FailOverCacheSyncBridge(const FailOverCacheSyncBridge&) = delete;

    FailOverCacheSyncBridge&
    operator=(const FailOverCacheSyncBridge&) = delete;

    ~FailOverCacheSyncBridge() = default;

    virtual void
    initialize(Volume* vol);

    virtual const char*
    getName() const
    {
        return "FailOverCacheSyncBridge";
    }

    virtual void
    destroy(SyncFailOverToBackend);

    virtual bool
    addEntries(const std::vector<ClusterLocation>& locs,
               size_t num_locs,
               uint64_t start_address,
               const uint8_t* data);

    virtual void
    newCache(std::unique_ptr<FailOverCacheProxy> cache);

    virtual void
    setRequestTimeout(const uint32_t seconds);

    virtual void
    removeUpTo(const SCO& sconame);

    virtual uint64_t
    getSCOFromFailOver(SCO sconame,
                       SCOProcessorFun processor);

    virtual void
    Flush();

    virtual void
    Clear();

    virtual bool
    isMode(FailOverCacheMode mode);

    void
    handleException(std::exception& e,
                    const char* where);

private:
    DECLARE_LOGGER("FailOverCacheSyncBridge");

    ClusterSize cluster_size_;
    ClusterMultiplier cluster_multiplier_;
    bool stop_;
    Volume* vol_ = { nullptr };

};

}

#endif // VD_FAILOVER_CACHE_SYNCBRIDGE

// Local Variables: **
// mode: c++ **
// End: **
