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

#include "LockCommunicator.h"

namespace backend
{

const std::string
LockCommunicator::lock_name_("lock_name");

LockCommunicator::LockCommunicator(GlobalLockService& lock_service,
                                           const boost::posix_time::time_duration connection_timeout,
                                           const boost::posix_time::time_duration interrupt_timeout)
    : lock_service_(lock_service)
    , bi_(lock_service_.cm_->newBackendInterface(lock_service_.ns_, 0))
    , connection_timeout_(connection_timeout)
    , lock_(connection_timeout,
            interrupt_timeout)
{}

bool
LockCommunicator::namespace_exists()
{
    try
    {
        return bi_->namespaceExists();
    }
    catch(std::exception& e)
    {
        LOG_INFO("Could not find out whether namespace " << ns() <<
                 " exists: " << e.what());
        return false;
    }
}

bool
LockCommunicator::lock_exists()
{
    return bi_->objectExists(lock_name_);
}

void
LockCommunicator::freeLock()
{
    lock_.hasLock = false;
    overwriteLock();
}

Lock
LockCommunicator::getLock()
{
    LOG_INFO("Getting the lock and updateding the etag for namespace " << ns() );

    std::string str;
    etag_ = bi_->x_read(str,
                        lock_name_,
                        InsistOnLatestVersion::T).etag_;

    LOG_TRACE("Leaving, new etag: " << etag_.str() << " for namespace " << ns());
    return Lock(str);
}

bool
LockCommunicator::overwriteLock()
{
    LOG_INFO("Overwriting the lock with etag " << etag_.str() << " for namespace " << ns());
    try
    {
        std::string str;
        lock_.save(str);
        etag_ = bi_->x_write(str,
                             lock_name_,
                             OverwriteObject::T,
                             &etag_).etag_;

        LOG_INFO("Overwrote the lock for namespace " << ns());
        return true;
    }
    catch(const std::exception& e)
    {
        LOG_INFO("Exception while overwriting the lock " << e.what() << " for namespace " << ns());
        return false;
    }
    catch(...)
    {
        LOG_INFO("Unknown exception while overwriting the lock for namespace" << ns());
        return false;
    }
}

bool
LockCommunicator::putLock()
{
    LOG_INFO("Putting the lock");
    try
    {
        std::string str;
        lock_.save(str);
        etag_ = bi_->x_write(str,
                             lock_name_,
                             OverwriteObject::F).etag_;

        LOG_INFO("put the lock");
        return true;
    }
    catch(const std::exception& e)
    {
        LOG_WARN("Exception while putting the lock " << e.what());
        return false;
    }
    catch(...)
    {
        LOG_WARN("Unknown exception while putting the lock ");
        return false;
    }
}

bool
LockCommunicator::TryToAcquireLock()
{
    LOG_INFO("Trying to Acquire the Lock for namespace " << ns());
    try
    {
        Lock l = getLock();
        if(not l.hasLock)
        {
            LOG_INFO("got the lock for namespace " << ns() << ", was unlocked" );
            return overwriteLock();
        }
        else
        {
            boost::this_thread::sleep(l.get_timeout());
            LOG_INFO("Trying to stealing the lock with etag: " << etag_.str() << " for namespace " << ns());
            return overwriteLock();
        }
    }
    catch(std::exception& e)
    {
        LOG_WARN("Exception while trying to acquire the lock " << e.what() << " for namespace " << ns());
        return false;
    }
    catch(...)
    {
        LOG_WARN("Unknown exception while trying to acquire the lock for namespace " << ns());
        return false;
    }
}

bool
LockCommunicator::Update(duration_type max_wait_time)
{
    LOG_INFO("Update of lock for " << ns());

    ++lock_;
    uint64_t count = 0;
    clock_type::time_point start_of_update = clock_type::now();

    while (count++ < max_update_retries)
    {
        LOG_INFO("Once more into the breach dear friends, for namespace " << ns() << " for the " << count << "th time");

        if(overwriteLock())
        {
            LOG_INFO("Update of lock for " << ns() << " without problem");
            return true;
        }
        else
        {
            LOG_WARN("Update of lock for " << ns() << " failed");
            try
            {
                boost::optional<Lock> local_lock;
                const int sleep_seconds = 1;

                while(true)
                {
                    try
                    {
                        LOG_INFO("Reading the lock to check we're still owner");
                        local_lock.reset(getLock());
                        break;
                    }
                    catch(...)
                    {
                        clock_type::duration update_duration = clock_type::now() - start_of_update;
                        if(update_duration + boost::chrono::seconds(sleep_seconds) > max_wait_time)
                        {
                            throw;
                        }
                        else
                        {
                            LOG_WARN("Reading failed, we have " << boost::chrono::duration_cast<boost::chrono::seconds>(max_wait_time - update_duration) << " left");
                        }
                    }
                    //boost::this_thread::sleep uses get_system_time() and might hang on system time reset
                    sleep(sleep_seconds);
                }
                VERIFY(local_lock);
                if(local_lock->different_owner(lock_))
                {
                    LOG_WARN("Lost the lock for " << ns());
                    return false;
                }
            }

            catch(const std::exception& e)
            {
                LOG_WARN("Getting the lock for " << ns() << " threw exception " << e.what());
                return false;
            }
            catch(...)
            {
                LOG_WARN("Getting the lock for " << ns() << " threw unknown exception ");
                return false;
            }
        }
    }
    LOG_ERROR("Ran out of retries for namespace " << ns());
    return false;
}

}

// Local Variables: **
// mode: c++ **
// End: **
