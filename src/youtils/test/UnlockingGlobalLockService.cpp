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

#include "UnlockingGlobalLockService.h"
#include "boost/foreach.hpp"

namespace youtilstest
{

boost::mutex
UnlockingGlobalLockService::unlocking_threads_mutex_;

std::map<std::string, boost::thread* >
UnlockingGlobalLockService::unlocking_threads_;

boost::mutex
UnlockingGlobalLockService::joinable_threads_mutex_;

boost::condition_variable
UnlockingGlobalLockService::joinable_threads_condition_variable_;

std::list<boost::thread*>
UnlockingGlobalLockService::joinable_threads_;

std::unique_ptr<boost::thread>
UnlockingGlobalLockService::thread_joiner_;

UnlockingGlobalLockService::UnlockingGlobalLockService(const GracePeriod& grace_period,
                                                       lost_lock_callback callback,
                                                       void* data,
                                                       const std::string& ns,
                                                       const boost::posix_time::time_duration& unlock_timeout)
    : GlobalLockService(grace_period,
                        callback,
                        data)
    , ns_(ns)
    , unlock_timeout_(unlock_timeout)
{
    thread_joiner_.reset(new boost::thread(boost::ref(*this)));
}


UnlockingGlobalLockService::~UnlockingGlobalLockService()
{

    {
        boost::unique_lock<boost::mutex> lock(unlocking_threads_mutex_);
        BOOST_FOREACH(auto pair, unlocking_threads_)
        {
            pair.second->interrupt();
        }
    }
    while(not unlocking_threads_.empty())
    {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }

    while(not joinable_threads_.empty())
    {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        // safety notify
        joinable_threads_condition_variable_.notify_all();
    }
    thread_joiner_->interrupt();
    thread_joiner_->join();
}

bool
UnlockingGlobalLockService::lock()
{

    {
        boost::unique_lock<boost::mutex> lock(unlocking_threads_mutex_);
        if(unlocking_threads_.find(ns_) != unlocking_threads_.end())
        {
            return false;
        }
        else
        {
            UnlockCallback cb(ns_,
                              callback_,
                              data_,
                              *this);
            unlocking_threads_[ns_] = new boost::thread(cb);
            return true;
        }
    }
}

void
UnlockingGlobalLockService::UnlockCallback::operator()()
{
    try
    {
        boost::this_thread::sleep(service_.unlock_timeout_);
        {
            service_.finish_thread(lock_name_,
                                  callback_,
                                  data_);
        }
    }
    catch(boost::thread_interrupted&)
    {
        service_.finish_thread(lock_name_);
    }
}

void
UnlockingGlobalLockService::operator()()
{
    try
    {
        while(true)
        {
            boost::unique_lock<boost::mutex> lock(joinable_threads_mutex_);
            joinable_threads_condition_variable_.wait(lock);
            auto it = joinable_threads_.begin();
            while(it != joinable_threads_.end())
            {
                (*it)->join();
                delete *it;
                it = joinable_threads_.erase(it);
            }
        }
    }
    catch(boost::thread_interrupted& )
    {
        return;
    }
    catch(std::exception& e)
    {
        LOG_ERROR("exception escaping the join thread, should not happen " << e.what());
    }
    catch(...)
    {
        LOG_ERROR("unknown exception escaping the join thread, should not happen ");
    }
}


void
UnlockingGlobalLockService::finish_thread(const std::string& ns,
                                          lost_lock_callback callback,
                                          void* data)
{
    boost::unique_lock<boost::mutex> lock(unlocking_threads_mutex_);
    lock_iterator it = unlocking_threads_.find(ns);
    if(it != unlocking_threads_.end())
    {
        if(callback)
        {
            callback(data,
                     "You've lost the lock... be a man");
        }
        {
            boost::unique_lock<boost::mutex> lock(joinable_threads_mutex_);
            joinable_threads_.push_back(it->second);
            unlocking_threads_.erase(it);
        }
        joinable_threads_condition_variable_.notify_all();
    }
}

void
UnlockingGlobalLockService::unlock()
{
        finish_thread(ns_);
}
}


// Local Variables: **
// bvirtual-targets: ("bin/backup_volume_driver") **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 4" **
// mode: c++ **
// End: **
