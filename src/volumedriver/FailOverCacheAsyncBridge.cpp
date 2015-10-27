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

#include "FailOverCacheAsyncBridge.h"
#include "Volume.h"

#include <algorithm>
#include <cerrno>

namespace volumedriver
{

FailOverCacheAsyncBridge::FailOverCacheAsyncBridge(const std::atomic<unsigned>& max_entries,
                                                   const std::atomic<unsigned>& write_trigger)
    : mutex_("FailOverCacheAsyncBridge", fungi::Mutex::ErrorCheckingMutex)
    , cluster_size_(VolumeConfig::default_cluster_size())
    , cluster_multiplier_(VolumeConfig::default_cluster_multiplier())
    , newData(cluster_size_ * max_entries)
    , oldData(cluster_size_ * max_entries)
    , initial_max_entries_(max_entries)
    , condvar_(mutex_)
    , max_entries_(max_entries)
    , write_trigger_(write_trigger)
    , thread_(nullptr)
    , stop_(true)
    , newOnesMutex_("NewOnes")
    , throttling(false)
{
}

void
FailOverCacheAsyncBridge::initCache()
{
    //PRECONDITION: thread_ = 0, oldOnes = 0, newones = 0
    // Y42 why not assert for these preconditions??
    // Y42 also a throw might put a monkey wrench into these so called preconditions
    if(cache_)
    {
        thread_ = new fungi::Thread(*this,false);
        stop_ = false;
        thread_->StartWithoutCleanup();
    }
}

void
FailOverCacheAsyncBridge::initialize(Volume* vol)
{
    //SHOULD NEVER BE CALLED TWICE as it breaks initcache's preconditions
    ASSERT(not vol_);
    // ASSERT(vol);
    vol_ = vol;
    initCache();
}

const char*
FailOverCacheAsyncBridge::getName() const
{
    return "FailOverCacheAsyncBridge";
}

bool
FailOverCacheAsyncBridge::backup()
{
    return cache_ != 0;
}

fungi::Mutex&
FailOverCacheAsyncBridge::getMutex()
{
    return mutex_;
}

std::unique_ptr<FailOverCacheProxy>&
FailOverCacheAsyncBridge::getCache()
{
    return cache_;
}

void
FailOverCacheAsyncBridge::newCache(std::unique_ptr<FailOverCacheProxy> cache)
{
    LOG_INFO("newCache");

    {
        fungi::ScopedLock l(mutex_);
        stop_ = true;
        condvar_.signal_no_lock();
    }
    if(cache_)
    {
        cache_->delete_failover_dir();

        thread_->join();
        thread_->destroy();
        thread_ = nullptr;

        {
            fungi::ScopedLock l1(mutex_);
            fungi::ScopedLock l(newOnesMutex_);
            newOnes.clear();
            oldOnes.clear();
        }
    }

    cache_ = std::move(cache);

    initCache();
}

// Called from Volume::destroy
void
FailOverCacheAsyncBridge::destroy(SyncFailOverToBackend sync)
{

    {
        fungi::ScopedLock l(mutex_);
        stop_ = true;
        condvar_.signal_no_lock();
    }
    if (cache_)
    {

        thread_->join();
        thread_->destroy();
        thread_ = 0;
        // VOLUME SHOULD HAVE BEEN SYNCED AND DETACHED
        //  VERIFY(oldOnes.empty() and
        // newOnes.empty());
        {
            fungi::ScopedLock l(newOnesMutex_);
            if (not oldOnes.empty())
            {
                if(T(sync))
                {
                    try
                    {
                        cache_->addEntries(oldOnes);
                    }
                    // Z42: std::exception?
                    catch(fungi::IOException& e)
                    {
                        LOG_ERROR("problem adding entries " << e.what() << ", ignoring");
                    }

                }
                oldOnes.clear();
            }
        }

        {
            fungi::ScopedLock l(newOnesMutex_);

            if(not newOnes.empty())
            {
                if(T(sync))
                {
                    try
                    {
                        cache_->addEntries(newOnes);
                    }
                    // Z42: std::exception?
                    catch(fungi::IOException& e)
                    {
                        LOG_ERROR("problem adding entries " << e.what() << ", ignoring");
                    }

                }
                newOnes.clear();

            }
        }
        {

            fungi::ScopedLock l(mutex_);
            cache_ = nullptr;
        }

    }
}

void
FailOverCacheAsyncBridge::setRequestTimeout(const uint32_t seconds)
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        cache_->setRequestTimeout(seconds);
    }
}

void
FailOverCacheAsyncBridge::run()
{
    fungi::ScopedLock l(mutex_);
    while (not stop_)
    {
        try
        {
            if (not oldOnes.empty())
            {
                LOG_DEBUG("Writing " << oldOnes.size() << " entries to the failover cache");
                cache_->addEntries(oldOnes);
                oldOnes.clear();
                LOG_DEBUG("Written ");
            }
            else
            {
                // Y42 The machine that goes ping
                cache_->flush();
            }
            condvar_.wait_sec_no_lock(timeout_);

            if (oldOnes.empty())
            {
                //we get here by a timeout -> time to swap
                //no risk for deadlock as addEntry only does a tryLock on mutex_
                fungi::ScopedLock foo(newOnesMutex_);
                std::swap(newOnes, oldOnes);
                std::swap(newData, oldData);
            }
        }
        catch (std::exception& e)
        {
            LOG_ERROR("Exception in failover thread: " << e.what());
            if(vol_)
            {
                vol_->setVolumeFailOverState(VolumeFailOverState::DEGRADED);
            }

            fungi::ScopedLock l(newOnesMutex_);

            stop_ = true;
            newOnes.clear();
            oldOnes.clear();

            // This thread cleans up itself when it is detached
            thread_->detach();
            thread_ = nullptr;
            cache_ = nullptr;
        }
        // Z42: catch (...) ?
    }
}

// Y42: This needs to be fixed!!!
void
FailOverCacheAsyncBridge::setThrottling(bool v)
{
    if(v and (not throttling))
    {
        LOG_DEBUG("Slowing down write operations because the FailOverCache is not able to sustain the write speed");
    }
    if((not v) and throttling)
    {
        LOG_DEBUG("Resuming write operations since the FailOverCache is able to sustain the write speed");
    }
    throttling = v;
}

bool
FailOverCacheAsyncBridge::addEntry(ClusterLocation loc,
                                   uint64_t lba,
                                   const uint8_t* buf,
                                   size_t bufsize)
{
    if (not stop_)
    {
    	setThrottling(newOnes.size() >= max_entries_);

    	if(not throttling)
    	{
            uint8_t *ptr = newData.data() + (newOnes.size() * cluster_size_);
            memcpy(ptr, buf, cluster_size_);
            newOnes.emplace_back(loc, lba, ptr, cluster_size_);
    	}

    	if (newOnes.size() >= write_trigger_)
    	{
            //must be trylock otherwize risk for deadlock
    	    if(mutex_.try_lock())
            {
                if (oldOnes.empty())
                {
                    std::swap(newOnes, oldOnes);
                    std::swap(newData, oldData);
                    condvar_.signal_no_lock();
                }
                mutex_.unlock();
            }
        }
        return not throttling;
    }
    else
    {
        return true;
    }
}

bool
FailOverCacheAsyncBridge::addEntries(const std::vector<ClusterLocation>& locs,
                                     size_t num_locs,
                                     uint64_t start_address,
                                     const uint8_t* data)
{
    VERIFY(initial_max_entries_ == max_entries_);
    fungi::ScopedLock l(newOnesMutex_);

    // Needs improvement!!!
    if (stop_)
    {
        return true;
    }
    // Just see if there is enough space for the whole batch
    setThrottling(max_entries_ - newOnes.size() < locs.size());
    if (throttling)
    {
        return false;
    }
    // Otherwise work the batch
    uint64_t lba = start_address;
    for (size_t i = 0; i < num_locs; i++)
    {
        addEntry(locs[i], lba, data + i * cluster_size_, cluster_size_);
        lba += cluster_multiplier_;
    }
    return true;
}

void FailOverCacheAsyncBridge::Flush()
{
    fungi::ScopedLock l(mutex_);
    Flush_();
}

void
FailOverCacheAsyncBridge::Flush_()
{
    if(cache_)
    {
        {
            fungi::ScopedLock l(newOnesMutex_);
            LOG_DEBUG("oldOnes: " << oldOnes.size() << " ,newOnes: " << newOnes.size());

            oldOnes.reserve(oldOnes.size() + newOnes.size());
            for (auto&& v : newOnes)
            {
                oldOnes.emplace_back(std::move(v));
            }

            newOnes.clear();
            VERIFY(newOnes.empty());
        }
        try
        {
            cache_->addEntries(std::move(oldOnes));
            cache_->flush();
        }
        // Z42: std::exception?
        catch(fungi::IOException& e)
        {
            LOG_ERROR("IOException while flushing the failover cache, willbe retriggered in the run thread");
        }
    }
}

void
FailOverCacheAsyncBridge::removeUpTo(const SCO& sconame)
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        cache_->removeUpTo(sconame);
    }
}

void
FailOverCacheAsyncBridge::Clear()
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        cache_->clear();
    }
}

uint64_t
FailOverCacheAsyncBridge::getSCOFromFailOver(SCO sconame,
                                             SCOProcessorFun processor)
{
    fungi::ScopedLock l(mutex_);
    Flush_(); // Z42: too much overhead?

    if (cache_)
    {
        return cache_->getSCOFromFailOver(sconame,
                                          processor);
    }
    else
    {
        throw FailOverCacheNotConfiguredException();
    }
}

FailOverCacheMode
FailOverCacheAsyncBridge::mode() const
{
    return FailOverCacheMode::Asynchronous;
}

}

// Local Variables: **
// mode: c++ **
// End: **
