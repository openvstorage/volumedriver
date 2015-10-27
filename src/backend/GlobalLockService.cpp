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

#include "BackendException.h"
#include "GlobalLockService.h"
#include "HeartBeat.h"

#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>

namespace backend
{

GlobalLockService::GlobalLockService(const youtils::GracePeriod& grace_period,
                                     lost_lock_callback callback,
                                     void* data,
                                     BackendConnectionManagerPtr cm,
                                     const UpdateInterval& update_interval,
                                     const Namespace& ns)
    : youtils::GlobalLockService(grace_period,
                                 callback,
                                 data)
    , cm_(cm)
    , ns_(ns)
    , update_interval_(update_interval)

{
    const auto ext_api = cm_->newBackendInterface(ns_)->hasExtendedApi();
    if (not ext_api)
    {
        LOG_ERROR("Global lock server cannot be created as the configured backend doesn't support the extended API");
        throw BackendNotImplementedException();
    }
}

GlobalLockService::~GlobalLockService()
{
    unlock(std::string("Destructor of GlobalLockService for namespace ") + ns_.str());
    LOG_INFO("Destructor finished");
}

boost::posix_time::time_duration
GlobalLockService::get_session_wait_time() const
{
    // 1 update_interval as time_out for the connection
    // 1 update interval as heart_beat timeout
    // 1 grace period to make sure the executable is stopped
    return update_interval_ + update_interval_ + grace_period_;
}

bool
GlobalLockService::lock()
{

    boost::lock_guard<boost::mutex> lock(heartbeat_thread_mutex_);
    if(heartbeat_thread_)
    {
        return false;
    }

    HeartBeat heartbeat(*this,
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
GlobalLockService::do_callback(const std::string& reason)
{
    if(callback_)
    {
        LOG_INFO("Calling callback for namespace " << ns_);
        try
        {
            callback_(data_,
                      reason);
        }
        catch(...)
        {
            LOG_FATAL("Got an exception while notifying of a lost lock, "
                      << "hold on to your seats as we attempt an emergency landing");

            std::unexpected();
        }
    }
}

void
GlobalLockService::finish_thread()
{
    LOG_INFO("finishing thread for " << ns_ << " because we lost the lock");

    boost::lock_guard<boost::mutex> lock(heartbeat_thread_mutex_);
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
GlobalLockService::unlock()
{
    unlock("Unlock by client");
}

void
GlobalLockService::unlock(const std::string& reason)
{
    LOG_INFO("Got an unlock for namespace " << ns_ << ", reason: " << reason);
    boost::lock_guard<boost::mutex> lock(heartbeat_thread_mutex_);
    if(heartbeat_thread_)
    {
        heartbeat_thread_->interrupt();
        do_callback("unlock called");
        LOG_INFO("joining heartbeat thread for " << ns_);
        heartbeat_thread_->join();
        LOG_INFO("exit finishing thread for " << ns_);
        heartbeat_thread_.reset(0);

    }
    VERIFY(not heartbeat_thread_);
}

}

// Local Variables: **
// mode: c++ **
// End: **
