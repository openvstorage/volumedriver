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

#include "Catchers.h"
#include "HeartBeatLockService.h"
#include "HeartBeat.h"

#include <youtils/System.h>

namespace youtils
{

#define LOCK()                                                \
    boost::lock_guard<decltype(heartbeat_thread_mutex_)> hbtg__(heartbeat_thread_mutex_)

HeartBeatLockService::HeartBeatLockService(const GracePeriod& grace_period,
                                           lost_lock_callback callback,
                                           void* data,
                                           GlobalLockStorePtr lock_store,
                                           const UpdateInterval& update_interval)
    : GlobalLockService(grace_period,
                        callback,
                        data)
    , lock_store_(lock_store)
    , update_interval_(update_interval)
{}

HeartBeatLockService::~HeartBeatLockService()
{
    unlock("Destruction of HeartBeatLockService");
    LOG_INFO(name() << ": destructed");
}

bool
HeartBeatLockService::lock()
{
    LOCK();

    if(heartbeat_thread_)
    {
        return false;
    }

    HeartBeat heartbeat(lock_store_,
                        [&]()
                        {
                            finish_thread();
                        },
                        update_interval_,
                        get_session_wait_time());

    if(heartbeat.grab_lock())
    {
        VERIFY(not heartbeat_thread_);
        heartbeat_thread_.reset(new boost::thread(std::move(heartbeat)));
        return true;
    }
    else
    {
        return false;
    }
}

void
HeartBeatLockService::unlock()
{
    unlock("Unlock by client");
}

void
HeartBeatLockService::finish_thread()
{
    LOG_INFO(name() << ": finishing thread because we lost the lock");

    LOCK();
    if(heartbeat_thread_)
    {
        do_callback("lost the lock");
        // Detaching the heartbeat thread
        std::unique_ptr<boost::thread> tmp;
        tmp.swap(heartbeat_thread_);
        tmp->detach();
        VERIFY(not heartbeat_thread_);
    }
}

void
HeartBeatLockService::do_callback(const std::string& reason)
{
    if(callback_)
    {
        LOG_INFO(name() << ": calling callback");
        try
        {
            callback_(data_,
                      reason);
        }
        CATCH_STD_ALL_EWHAT({
                LOG_FATAL("Got an exception while notifying of a lost lock: " <<
                          EWHAT << ". Hold on to your seats as we attempt an emergency landing");
                std::unexpected();
            });
    }
}

void
HeartBeatLockService::unlock(const std::string& reason)
{
    LOG_INFO(name() << ": got an unlock request, reason: " << reason);
    LOCK();
    if(heartbeat_thread_)
    {
        heartbeat_thread_->interrupt();
        do_callback("unlock called");
        LOG_INFO(name() << ": joining heartbeat thread");
        heartbeat_thread_->join();
        LOG_INFO(name() << ": destructing thread");
        heartbeat_thread_.reset(0);

    }
    VERIFY(not heartbeat_thread_);
}

// Returns the time another contender has to wait before trying to grab the lock.
boost::posix_time::time_duration
HeartBeatLockService::get_session_wait_time() const
{
    // 1 update_interval as time_out for the connection
    // 1 update interval as heart_beat timeout
    // 1 grace period to make sure the executable is stopped
    return update_interval_ + update_interval_ + grace_period_;
}

}
