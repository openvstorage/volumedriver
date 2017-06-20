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

#include "failovercache/ClientInterface.h"

#include <algorithm>
#include <cerrno>

#include <youtils/InlineExecutor.h>

namespace volumedriver
{

namespace bc = boost::chrono;
namespace yt = youtils;

#define LOCK()                                          \
    boost::lock_guard<decltype(mutex_)> lg__(mutex_)

FailOverCacheSyncBridge::FailOverCacheSyncBridge(const size_t max_entries)
    : FailOverCacheBridgeInterface(max_entries)
{}

void
FailOverCacheSyncBridge::initialize(DegradedFun fun)
{
    LOCK();

    ASSERT(not degraded_fun_);
    degraded_fun_ = std::move(fun);
}

bool
FailOverCacheSyncBridge::backup()
{
    return cache_ != nullptr;
}

void
FailOverCacheSyncBridge::newCache(std::unique_ptr<failovercache::ClientInterface> cache)
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
            cache_->setRequestTimeout(bc::duration_cast<bc::milliseconds>(seconds));
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

template<typename... Args>
boost::future<void>
FailOverCacheSyncBridge::wrap_(const char* desc,
                               boost::future<void> (failovercache::ClientInterface::*fn)(Args...),
                               Args... args)
{
    boost::future<void> f;

    {
        LOCK();
        if(cache_)
        {
            try
            {
                f = (cache_.get()->*fn)(std::forward<Args>(args)...);
            }
            CATCH_STD_ALL_EWHAT({
                    handleException(EWHAT,
                                    desc);
                    return boost::make_ready_future();
                });
        }
        else
        {
            return boost::make_ready_future();
        }
    }

    ASSERT(f.valid());
    return f.then(yt::InlineExecutor::get(),
                  [desc,
                   this](boost::future<void> f)
                  {
                      try
                      {
                          f.get();
                      }
                      CATCH_STD_ALL_EWHAT({
                              LOCK();
                              handleException(EWHAT,
                                              desc);
                          });
                  });
}

boost::future<void>
FailOverCacheSyncBridge::addEntries(const std::vector<ClusterLocation>& locs,
                                    uint64_t start_address,
                                    const uint8_t* data)
{
    return wrap_<decltype(locs),
                 decltype(start_address),
                 decltype(data)>("add entries",
                                 &failovercache::ClientInterface::addEntries,
                                 locs,
                                 start_address,
                                 data);
}

boost::future<void>
FailOverCacheSyncBridge::Flush()
{
    return wrap_("flush",
                 &failovercache::ClientInterface::flush);
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
FailOverCacheSyncBridge::handleException(const char* what,
                                         const char* where)
{
    ASSERT_LOCKABLE_LOCKED(mutex_);

    LOG_ERROR("Exception in " << where << ": " << what);
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
