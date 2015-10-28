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

#include "FailOverCacheSyncBridge.h"
#include "Volume.h"

#include <algorithm>
#include <cerrno>

namespace volumedriver
{

FailOverCacheSyncBridge::FailOverCacheSyncBridge()
    : mutex_("FailOverCacheSyncBridge", fungi::Mutex::ErrorCheckingMutex)
    , cluster_size_(VolumeConfig::default_cluster_size())
    , cluster_multiplier_(VolumeConfig::default_cluster_multiplier())
{
}

void
FailOverCacheSyncBridge::initialize(Volume* vol)
{
    ASSERT(not vol_);
    vol_ = vol;
}

const char*
FailOverCacheSyncBridge::getName() const
{
    return "FailOverCacheSyncBridge";
}

bool
FailOverCacheSyncBridge::backup()
{
    return cache_ != 0;
}

void
FailOverCacheSyncBridge::newCache(std::unique_ptr<FailOverCacheProxy> cache)
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        cache_->delete_failover_dir();
    }
    cache_ = std::move(cache);
}

// Called from Volume::destroy
void
FailOverCacheSyncBridge::destroy(SyncFailOverToBackend sync)
{
    fungi::ScopedLock l(mutex_);
    cache_ = nullptr;
}

void
FailOverCacheSyncBridge::setRequestTimeout(const uint32_t seconds)
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        try
        {
            cache_->setRequestTimeout(seconds);
        }
        catch (std::exception& e)
        {
            handleException(e, "setRequestTimeout");
        }
    }
}

bool
FailOverCacheSyncBridge::addEntries(const std::vector<ClusterLocation>& locs,
                                    size_t num_locs,
                                    uint64_t start_address,
                                    const uint8_t* data)
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        std::vector<FailOverCacheEntry> entries;
        entries.reserve(num_locs);
        uint64_t lba = start_address;
        for (size_t i = 0; i < num_locs; i++)
        {
            entries.emplace_back(locs[i], lba, data + i * cluster_size_, cluster_size_);
            lba += cluster_multiplier_;
        }
        try
        {
            cache_->addEntries(std::move(entries));
        }
        catch (std::exception& e)
        {
            handleException(e, "addEntries");
        }
    }
    return true;
}

void FailOverCacheSyncBridge::Flush()
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        try
        {
            cache_->flush();
        }
        catch (std::exception& e)
        {
            handleException(e, "Flush");
        }
    }
}

void
FailOverCacheSyncBridge::removeUpTo(const SCO& sconame)
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        try
        {
            cache_->removeUpTo(sconame);
        }
        catch (std::exception& e)
        {
            handleException(e, "removeUpTo");
        }
    }
}

void
FailOverCacheSyncBridge::Clear()
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        try
        {
            cache_->clear();
        }
        catch (std::exception& e)
        {
            handleException(e, "Clear");
        }
    }
}

uint64_t
FailOverCacheSyncBridge::getSCOFromFailOver(SCO sconame,
                                            SCOProcessorFun processor)
{
    uint64_t ret;
    fungi::ScopedLock l(mutex_);
    if (cache_)
    {
        try
        {
            cache_->flush(); // Z42: too much overhead?
            ret = cache_->getSCOFromFailOver(sconame,
                                             processor);
        }
        catch (std::exception& e)
        {
            handleException(e, "getSCOFromFailOver");
        }
    }
    else
    {
        throw FailOverCacheNotConfiguredException();
    }
    return ret;
}

FailOverCacheMode
FailOverCacheSyncBridge::mode() const
{
    return FailOverCacheMode::Synchronous;
}

void
FailOverCacheSyncBridge::handleException(std::exception& e,
                                         const char* where)
{
    LOG_ERROR("Exception in FailOverCacheSyncBridge::" << where << " : " << e.what());
    if(vol_)
    {
        vol_->setVolumeFailOverState(VolumeFailOverState::DEGRADED);
    }
    cache_ = nullptr;
}

}

// Local Variables: **
// mode: c++ **
// End: **
