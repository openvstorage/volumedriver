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

#include "LockCommunicator.h"

namespace backend
{

namespace yt = youtils;

LockCommunicator::LockCommunicator(GlobalLockStorePtr lock_store,
                                   const boost::posix_time::time_duration connection_timeout,
                                   const boost::posix_time::time_duration interrupt_timeout)
    : lock_store_(lock_store)
    , lock_(connection_timeout,
            interrupt_timeout)
{}

bool
LockCommunicator::lock_exists()
{
    return lock_store_->exists();
}

void
LockCommunicator::freeLock()
{
    lock_.hasLock(false);
    overwriteLock();
}

Lock
LockCommunicator::getLock()
{
    LOG_INFO(name() << ": getting the lock and updating the tag");

    Lock lock(lock_);
    std::tie(lock, tag_) = lock_store_->read();

    LOG_INFO(name() << ": new tag " << tag_);
    return lock;
}

bool
LockCommunicator::overwriteLock()
{
    LOG_INFO(name() << ": overwriting the lock with tag " << tag_);
    try
    {
        tag_ = lock_store_->write(lock_,
                                 tag_);

        LOG_INFO(name() << ": overwrote the lock");
        return true;
    }
    CATCH_STD_ALL_EWHAT({
            LOG_INFO(name() <<
                     ": exception while overwriting the lock: " << EWHAT);
            return false;
        });
}

bool
LockCommunicator::putLock()
{
    LOG_INFO(name() << ": putting the lock");

    try
    {
        tag_ = lock_store_->write(lock_,
                                 boost::none);

        LOG_INFO(name() << ": put the lock");
        return true;
    }
    CATCH_STD_ALL_EWHAT({
            LOG_WARN(name() <<
                     ": exception while putting the lock: " << EWHAT);
            return false;
        });
}

bool
LockCommunicator::tryToAcquireLock()
{
    LOG_INFO(name() << ": trying to acquire the lock");
    try
    {
        Lock l(getLock());
        if(not l.hasLock())
        {
            LOG_INFO(name() << ": got the lock");
            return overwriteLock();
        }
        else
        {
            boost::this_thread::sleep(l.get_timeout());
            LOG_INFO(name() <<
                     ": trying to steal the lock with tag " << tag_);
            return overwriteLock();
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_WARN(name() <<
                     ": exception while trying to acquire the lock " << EWHAT);
            return false;
        });
}

bool
LockCommunicator::refreshLock(MilliSeconds max_wait_time)
{
    LOG_INFO(name() << ": refreshing the lock");

    ++lock_;
    uint64_t count = 0;
    Clock::time_point start_of_update = Clock::now();

    while (count++ < max_update_retries)
    {
        LOG_INFO(name() <<
                 ": once more into the breach dear friends (retry " <<
                 count << ")");

        if (overwriteLock())
        {
            LOG_INFO(name() <<
                     ": lock was successfully refreshed");
            return true;
        }
        else
        {
            LOG_WARN(name() << ": failed to refresh lock");
            try
            {
                boost::optional<Lock> local_lock;
                const int sleep_seconds = 1;

                while(true)
                {
                    try
                    {
                        LOG_INFO(name() <<
                                 ": reading the lock to see whether we're still owning it");
                        local_lock = (getLock());
                        break;
                    }
                    catch (...)
                    {
                        Clock::duration update_duration =
                            Clock::now() - start_of_update;
                        if (update_duration + boost::chrono::seconds(sleep_seconds) > max_wait_time)
                        {
                            throw;
                        }
                        else
                        {
                            LOG_WARN(name() <<
                                     ": reading the lock failed, we have " <<
                                     boost::chrono::duration_cast<boost::chrono::seconds>(max_wait_time - update_duration) << " left");
                        }
                    }
                    //boost::this_thread::sleep uses get_system_time() and might hang on system time reset
                    sleep(sleep_seconds);
                }

                VERIFY(local_lock);
                if(local_lock->different_owner(lock_))
                {
                    LOG_WARN(name() << ": lost the lock");
                    return false;
                }
            }
            CATCH_STD_ALL_EWHAT({
                    LOG_WARN(name() <<
                             ": exception getting the lock: " << EWHAT);
                    return false;
                });
        }
    }

    LOG_ERROR(name() << ": ran out of retries");
    return false;
}

const std::string&
LockCommunicator::name() const
{
    return lock_store_->name();
}

}

// Local Variables: **
// mode: c++ **
// End: **
