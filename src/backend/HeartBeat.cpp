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

#include "HeartBeat.h"
#include "GlobalLockService.h"

namespace backend
{

HeartBeat::HeartBeat(GlobalLockService& global_lock_service,
                             const boost::posix_time::time_duration& heartbeat_timeout,
                             const boost::posix_time::time_duration& interrupt_timeout)
  : global_lock_service_(global_lock_service)
  , lock_communicator_(global_lock_service_,
                       heartbeat_timeout,
                       interrupt_timeout)
  , heartbeat_timeout_(heartbeat_timeout)
{}

bool
HeartBeat::grab_lock()
{
    if(not lock_communicator_.namespace_exists())
    {
        LOG_INFO("Namespace " << global_lock_service_.ns_ << " Doesn't exist");
        return false;
    }

    try
    {
        if(lock_communicator_.lock_exists())
        {
            LOG_INFO("Lock exists for namespace " << global_lock_service_.ns_ << ", trying to grab it");

            if( not lock_communicator_.TryToAcquireLock())
            {
                LOG_INFO("Lock exists for namespace " << global_lock_service_.ns_ << ", could not grab it");
                return false;
            }
        }
        else
        {
            LOG_INFO("Lock doesn't exists for namespace " << global_lock_service_.ns_ << ", trying to put it");
            if(not lock_communicator_.putLock())
            {
                LOG_INFO("Lock doesn't exists for namespace " << global_lock_service_.ns_ << ", could not to put it");
                return false;
            }
        }
    }
    catch(std::exception& e)
    {
        LOG_INFO("Exception trying to create the lock object " << e.what());
        return false;
    }
    catch(...)
    {
        LOG_INFO("Unknown exception trying to create the lock object ");
        return false;
    }

    LOG_INFO("Acquired the lock for namespace " << global_lock_service_.ns_ << ":" <<
             boost::this_thread::get_id());
    return true;
}

void
HeartBeat::operator()()
{
    LOG_INFO("Heartbeat thread for " << global_lock_service_.ns_ << ", id " <<
             boost::this_thread::get_id());
    try
    {
        do
        {
            boost::this_thread::sleep(heartbeat_timeout_);
        }
        while(lock_communicator_.Update(boost::chrono::milliseconds(heartbeat_timeout_.total_milliseconds())));
    }
    catch(boost::thread_interrupted&)
    {
        // This is only triggered from the unlock call on GlobalLockService.
        // Cleanup happens there
        LOG_INFO("HeartBeat Thread interrupted for " << global_lock_service_.ns_);
        try
        {
            LOG_INFO("Freeing the lock for " << global_lock_service_.ns_);
            lock_communicator_.freeLock();
        }
        catch(...)
        {
            LOG_WARN("Couldn't free the lock for " << global_lock_service_.ns_);
        }
        return;

    }
    catch(std::exception& e)
    {
        LOG_ERROR("Exception in HeartBeat Thread for " << global_lock_service_.ns_ << ", " << e.what());

    }
    catch(...)
    {
        LOG_ERROR("Unknown exception in HeartBeat Thread for " << global_lock_service_.ns_ );
    }
    LOG_INFO("Lost the lock for namespace " << global_lock_service_.ns_ << ":" << boost::this_thread::get_id());
    // finish_thread will detach and cleanup the thread
    global_lock_service_.finish_thread();
}

}

// Local Variables: **
// mode: c++ **
// End: **
