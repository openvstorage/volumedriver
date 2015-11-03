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
#include "HeartBeat.h"

#include <boost/thread.hpp>

namespace youtils
{

HeartBeat::HeartBeat(GlobalLockStorePtr lock_store,
                     FinishThreadFun finish_thread_fun,
                     const TimeDuration& heartbeat_timeout,
                     const TimeDuration& interrupt_timeout)
    : finish_thread_fun_(std::move(finish_thread_fun))
    , lock_communicator_(lock_store,
                         heartbeat_timeout,
                         interrupt_timeout)
    , heartbeat_timeout_(heartbeat_timeout)
{}

const std::string&
HeartBeat::name() const
{
    return lock_communicator_.name();
}

void
HeartBeat::operator()()
{
    LOG_INFO(name() << ": heartbeat thread " << boost::this_thread::get_id());
    try
    {
        do
        {
            boost::this_thread::sleep(heartbeat_timeout_);
        }
        while(lock_communicator_.refreshLock(boost::chrono::milliseconds(heartbeat_timeout_.total_milliseconds())));
    }
    catch (boost::thread_interrupted&)
    {
        // This is only triggered from the unlock call on GlobalLockService.
        // Cleanup happens there
        LOG_INFO(name() << ": heartbeat thread interrupted");

        try
        {
            LOG_INFO(name() << ": freeing the lock");
            lock_communicator_.freeLock();
        }
        CATCH_STD_ALL_LOGLEVEL_IGNORE(name() << ": couldn't free the lock",
                                      WARN);
        return;
    }
    CATCH_STD_ALL_LOG_IGNORE(name() << ": exception in heartbeat thread");

    LOG_INFO(name() << ": lost the lock, thread " <<
             boost::this_thread::get_id());
    // will detach and cleanup the thread
    finish_thread_fun_();
}

bool
HeartBeat::grab_lock()
{
    try
    {
        if(lock_communicator_.lock_exists())
        {
            LOG_INFO(name() << ": lock exists, trying to grab it");

            if(not lock_communicator_.tryToAcquireLock())
            {
                LOG_INFO(name() << ": failed to grab existing lock");
                return false;
            }
        }
        else
        {
            LOG_INFO(name() << ": lock does not exist, trying to place it");
            if(not lock_communicator_.putLock())
            {
                LOG_INFO(name() << ": failed to put lock");
                return false;
            }
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_INFO(name() << ": exception while grabbing the lock: " << EWHAT);
            return false;
        });

    LOG_INFO(name() << ": acquired the lock, thread " <<
             boost::this_thread::get_id());
    return true;
}

}
