// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

#include "FailOverCacheSyncBridge.h"
#include "Volume.h"

#include <algorithm>
#include <cerrno>

namespace volumedriver
{

#define LOCK()                                          \
    boost::lock_guard<decltype(mutex_)> lg__(mutex_)

FailOverCacheSyncBridge::FailOverCacheSyncBridge(const size_t max_entries)
    : FailOverCacheClientInterface(max_entries)
{}

void
FailOverCacheSyncBridge::initialize(DegradedFun fun)
{
    LOCK();

    ASSERT(not degraded_fun_);
    degraded_fun_ = std::move(fun);
}

const char*
FailOverCacheSyncBridge::getName() const
{
    return "FailOverCacheSyncBridge";
}

bool
FailOverCacheSyncBridge::backup()
{
    return cache_ != nullptr;
}

void
FailOverCacheSyncBridge::newCache(std::unique_ptr<FailOverCacheProxy> cache)
{
    LOCK();

    if(cache_)
    {
        cache_->delete_failover_dir();
    }

    cache_ = std::move(cache);
}

// Called from Volume::destroy
void
FailOverCacheSyncBridge::destroy(SyncFailOverToBackend /*sync*/)
{
    LOCK();
    cache_ = nullptr;
}

void
FailOverCacheSyncBridge::setRequestTimeout(const boost::chrono::seconds seconds)
{
    LOCK();

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

void
FailOverCacheSyncBridge::setBusyLoopDuration(const boost::chrono::microseconds usecs)
{
    LOCK();

    if(cache_)
    {
        try
        {
            cache_->setBusyLoopDuration(usecs);
        }
        catch (std::exception& e)
        {
            handleException(e, "setBusyLoopDuration");
        }
    }
}

boost::future<void>
FailOverCacheSyncBridge::addEntries(const std::vector<ClusterLocation>& locs,
                                    uint64_t start_address,
                                    const uint8_t* data)
{
    LOCK();

    if(cache_)
    {
        const size_t cluster_size =
            static_cast<size_t>(cache_->lba_size()) *
            static_cast<size_t>(cache_->cluster_multiplier());

        std::vector<FailOverCacheEntry> entries;
        entries.reserve(locs.size());
        uint64_t lba = start_address;
        for (size_t i = 0; i < locs.size(); i++)
        {
            entries.emplace_back(locs[i],
                                 lba, data + i * cluster_size,
                                 cluster_size);

            lba += cache_->cluster_multiplier();
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

    return boost::make_ready_future();
}

boost::future<void>
FailOverCacheSyncBridge::Flush()
{
    LOCK();

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

    return boost::make_ready_future();
}

void
FailOverCacheSyncBridge::removeUpTo(const SCO& sconame)
{
    LOCK();

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
    LOCK();

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
    LOCK();

    if (cache_)
    {
        try
        {
            cache_->flush(); // Z42: too much overhead?
            return cache_->getSCOFromFailOver(sconame,
                                              processor);
        }
        catch (std::exception& e)
        {
            handleException(e, "getSCOFromFailOver");
            UNREACHABLE;
        }
    }
    else
    {
        throw FailOverCacheNotConfiguredException();
    }
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
    if(degraded_fun_)
    {
        degraded_fun_();
    }
    cache_ = nullptr;
}

}

// Local Variables: **
// mode: c++ **
// End: **
