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

#ifndef FAILOVER_CACHE_BRIDGE_H
#define FAILOVER_CACHE_BRIDGE_H

#include "FailOverCacheStreamers.h"
#include "FailOverCacheProxy.h"
#include "SCO.h"

#include "failovercache/fungilib/CondVar.h"
#include "failovercache/fungilib/Mutex.h"
#include "failovercache/fungilib/Runnable.h"
#include "failovercache/fungilib/Thread.h"

#include <youtils/FileDescriptor.h>
#include <youtils/IOException.h>

namespace volumedriver
{

class Volume;

BOOLEAN_ENUM(SyncFailOverToBackend);

class VolumeFailOverCacheAdaptor
{
public:
    VolumeFailOverCacheAdaptor(Volume& volume)
        : volume_(volume)
    {}

    void
    operator()(ClusterLocation cli,
               uint64_t lba,
               byte* buf,
               int32_t size);

    Volume& volume_;
};

// Z42: we want an exception hierarchy here.
class FailOverCacheNotConfiguredException
    : public fungi::IOException
{
public:
    FailOverCacheNotConfiguredException()
        : fungi::IOException("FailOverCache not configured")
    {}
};

class FailOverCacheBridge
    : public fungi::Runnable
{
    friend class VolManagerTestSetup;
    friend class FailOverCacheTester;

public:
    FailOverCacheBridge(const std::atomic<unsigned>& max_entries,
                        const std::atomic<unsigned>& write_trigger);

    FailOverCacheBridge(const FailOverCacheBridge&) = delete;

    FailOverCacheBridge&
    operator=(const FailOverCacheBridge&) = delete;

    void
    initialize(Volume* vol);

    virtual void
    run();

    virtual const char*
    getName() const
    {
        return "FailOverCacheBridge";
    }

    ~FailOverCacheBridge() = default;

    void
    destroy(SyncFailOverToBackend);

    bool
    addEntry(FailOverCacheEntry* e);

    bool
    backup()
    {
        return cache_ != 0;
    }

    void
    newCache(std::unique_ptr<FailOverCacheProxy> cache);

    void
    setRequestTimeout(const uint32_t seconds);

    void
    removeUpTo(const SCO& sconame);

    template<typename T>
    uint64_t
    getSCOFromFailOver(SCO sconame,
                       T& sink)
    {
        fungi::ScopedLock l(mutex_);
        Flush_(); // Z42: too much overhead?

        if (cache_)
        {
            return cache_->getSCOFromFailOver(sconame,
                                              sink);
        }
        else
        {
            throw FailOverCacheNotConfiguredException();
        }
    }

    void
    Flush();

    void
    Clear();

    static const uint32_t RequestTimeout; /* seconds */

private:
    DECLARE_LOGGER("FailOverCacheBridge");

    void
    setThrottling(bool v);

    void
    initCache();

    void
    Flush_();

    std::unique_ptr<boost::ptr_vector<FailOverCacheEntry>> newOnes;
    std::unique_ptr<boost::ptr_vector<FailOverCacheEntry>> oldOnes;

    std::unique_ptr<FailOverCacheProxy> cache_;
    fungi::Mutex mutex_;
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

#endif // FAILOVER_CACHE_BRIDGE

// Local Variables: **
// mode: c++ **
// End: **
