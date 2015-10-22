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

#ifndef VD_FAILOVER_CACHE_ASYNCBRIDGE_H
#define VD_FAILOVER_CACHE_ASYNCBRIDGE_H

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

class FailOverCacheAsyncBridge
    : public fungi::Runnable, public FailOverCacheClientInterface
{

public:
    FailOverCacheAsyncBridge(const std::atomic<unsigned>& max_entries,
                             const std::atomic<unsigned>& write_trigger);

    FailOverCacheAsyncBridge(const FailOverCacheAsyncBridge&) = delete;

    FailOverCacheAsyncBridge&
    operator=(const FailOverCacheAsyncBridge&) = delete;

    virtual void
    initialize(Volume* vol);

    virtual void
    run();

    virtual const char*
    getName() const
    {
        return "FailOverCacheAsyncBridge";
    }

    ~FailOverCacheAsyncBridge() = default;

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

private:
    DECLARE_LOGGER("FailOverCacheAsyncBridge");

    bool
    addEntry(ClusterLocation loc,
            uint64_t lba,
            const uint8_t* buf,
            size_t bufsize);

    void
    setThrottling(bool v);

    void
    initCache();

    void
    Flush_();

    std::vector<FailOverCacheEntry> newOnes;
    std::vector<FailOverCacheEntry> oldOnes;
    ClusterSize cluster_size_;
    ClusterMultiplier cluster_multiplier_;
    std::vector<uint8_t> newData;
    std::vector<uint8_t> oldData;
    unsigned initial_max_entries_;

    fungi::CondVar condvar_;

    const std::atomic<unsigned>& max_entries_;
    const std::atomic<unsigned>& write_trigger_;

    // try to write every second
    static const int timeout_ = 1;
    // number of entries before scheduling a write
    fungi::Thread* thread_;
    bool stop_;
    fungi::Mutex newOnesMutex_;
    bool throttling;

    Volume* vol_ = { nullptr };

};

}

#endif // VD_FAILOVER_CACHE_ASYNCBRIDGE

// Local Variables: **
// mode: c++ **
// End: **
