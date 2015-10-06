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

#include "FailOverCacheBridge.h"
#include "Volume.h"

#include <algorithm>
#include <cerrno>

namespace volumedriver
{

void
VolumeFailOverCacheAdaptor::operator()(ClusterLocation cli,
                                       uint64_t lba,
                                       byte* buf,
                                       int32_t size)
{
    volume_.processFailOverCacheEntry(cli, lba, buf, size);
}

const uint32_t FailOverCacheBridge::RequestTimeout = 60 ; /*seconds*/

FailOverCacheBridge::FailOverCacheBridge(const std::atomic<unsigned>& max_entries,
                                         const std::atomic<unsigned>& write_trigger)
    : newOnes(nullptr)
    , oldOnes(nullptr)
    , cache_(nullptr)
    , mutex_("FailOverCacheBridge", fungi::Mutex::ErrorCheckingMutex)
    , condvar_(mutex_)
    , max_entries_(max_entries)
    , write_trigger_(write_trigger)
    , thread_(nullptr)
    , stop_(true)
    , newOnesMutex_("NewOnes")
    , throttling(false)
{}

void
FailOverCacheBridge::initCache()
{
    //PRECONDITION: thread_ = 0, oldOnes = 0, newones = 0
    // Y42 why not assert for these preconditions??
    // Y42 also a throw might put a monkey wrench into these so called preconditions
    if(cache_)
    {
        newOnes = std::make_unique<boost::ptr_vector<FailOverCacheEntry>>();
        oldOnes = std::make_unique<boost::ptr_vector<FailOverCacheEntry>>();
        thread_ = new fungi::Thread(*this,false);
        stop_ = false;
        thread_->StartWithoutCleanup();
    }
}

void
FailOverCacheBridge::initialize(Volume* vol)
{
    //SHOULD NEVER BE CALLED TWICE as it breaks initcache's preconditions
    ASSERT(not vol_);
    // ASSERT(vol);
    vol_ = vol;
    initCache();
}

void
FailOverCacheBridge::newCache(std::unique_ptr<FailOverCacheProxy> cache)
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
            newOnes = nullptr;
            oldOnes = nullptr;
        }
    }

    cache_ = std::move(cache);

    initCache();
}

// Called from Volume::destroy
void
FailOverCacheBridge::destroy(SyncFailOverToBackend sync)
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
        //  VERIFY(oldOnes->empty() and
        // newOnes->empty());
        {
            fungi::ScopedLock l(newOnesMutex_);
            if (oldOnes and
                not oldOnes->empty())
            {
                if(T(sync))
                {
                    try
                    {
                        cache_->addEntries(*oldOnes);
                    }
                    // Z42: std::exception?
                    catch(fungi::IOException& e)
                    {
                        LOG_ERROR("problem adding entries " << e.what() << ", ignoring");
                    }

                }
                oldOnes->clear();
            }
        }

        {
            fungi::ScopedLock l(newOnesMutex_);

            if(newOnes and
               not newOnes->empty())
            {
                if(T(sync))
                {
                    try
                    {
                        cache_->addEntries(*newOnes);
                    }
                    // Z42: std::exception?
                    catch(fungi::IOException& e)
                    {
                        LOG_ERROR("problem adding entries " << e.what() << ", ignoring");
                    }

                }
                newOnes->clear();

            }
        }
        {

            fungi::ScopedLock l(mutex_);
            cache_ = nullptr;
        }

    }
}

void
FailOverCacheBridge::setRequestTimeout(const uint32_t seconds)
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        cache_->setRequestTimeout(seconds);
    }

}

void
FailOverCacheBridge::run()
{
    fungi::ScopedLock l(mutex_);
    while (not stop_)
    {
        try
        {
            if (not oldOnes->empty())
            {
                LOG_DEBUG("Writing " << oldOnes->size() << " entries to the failover cache");
                cache_->addEntries(*oldOnes);
                oldOnes->clear();
                LOG_DEBUG("Written ");
            }
            else
            {
                // Y42 The machine that goes ping
                cache_->flush();
            }
            condvar_.wait_sec_no_lock(timeout_);

            if (oldOnes->empty())
            {
                //we get here by a timeout -> time to swap
                //no risk for deadlock as addEntry only does a tryLock on mutex_
                fungi::ScopedLock foo(newOnesMutex_);
                std::swap(newOnes, oldOnes);
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
            newOnes = nullptr;
            oldOnes = nullptr;

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
FailOverCacheBridge::setThrottling(bool v)
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
FailOverCacheBridge::addEntry(FailOverCacheEntry* e)
{
    fungi::ScopedLock l(newOnesMutex_);
    if (not stop_)
    {
    	setThrottling(newOnes->size() >= max_entries_);

    	if(not throttling)
    	{
            newOnes->push_back(e);
    	}

    	if (newOnes->size() >= write_trigger_)
    	{
            //must be trylock otherwize risk for deadlock
    	    if(mutex_.try_lock())
            {
                if (oldOnes->empty())
                {
                    std::swap(newOnes, oldOnes);
                    condvar_.signal_no_lock();
                }
                mutex_.unlock();
            }
        }
        return not throttling;
    }
    else
    {
        delete e;
        return true;
    }
}

void FailOverCacheBridge::Flush()
{
    fungi::ScopedLock l(mutex_);
    Flush_();
}

void
FailOverCacheBridge::Flush_()
{
    if(cache_)
    {
        {
            VERIFY(newOnes and oldOnes);
            fungi::ScopedLock l(newOnesMutex_);
            LOG_DEBUG("oldOnes: " << oldOnes->size() << " ,newOnes: " << newOnes->size());
            oldOnes->transfer(oldOnes->end(), newOnes->begin(), newOnes->end(), *newOnes);
            VERIFY(newOnes->empty());
        }
        try
        {
            cache_->addEntries(*oldOnes);
            oldOnes->clear();
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
FailOverCacheBridge::removeUpTo(const SCO& sconame)
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        cache_->removeUpTo(sconame);
    }
}

void
FailOverCacheBridge::Clear()
{
    fungi::ScopedLock l(mutex_);
    if(cache_)
    {
        cache_->clear();
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
