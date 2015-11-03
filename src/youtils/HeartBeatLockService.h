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

#ifndef YT_HEARTBEAT_LOCK_SERVICE_H
#define YT_HEARTBEAT_LOCK_SERVICE_H

#include "GlobalLockStore.h"
#include "GlobalLockService.h"
#include "HeartBeat.h"
#include "TimeDurationType.h"
#include "UUID.h"
#include "WithGlobalLock.h"

#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace youtils
{

DECLARE_DURATION_TYPE(UpdateInterval)

class HeartBeatLockService
    : public GlobalLockService
{
public:
    HeartBeatLockService(const youtils::GracePeriod& grace_period,
                         lost_lock_callback callback,
                         void* data,
                         GlobalLockStorePtr lock_store,
                         const UpdateInterval& update_interval);

    virtual ~HeartBeatLockService();

    const std::string&
    name() const
    {
        return lock_store_->name();
    }

    virtual bool
    lock() override;

    virtual void
    unlock() override;

    template<ExceptionPolicy exception_policy,
             typename Callable,
             std::string(Callable::*info_member_function)() = &Callable::info>
    using WithGlobalLock = youtils::WithGlobalLock<exception_policy,
                                                   Callable,
                                                   info_member_function,
                                                   HeartBeatLockService,
                                                   GlobalLockStorePtr,
                                                   const UpdateInterval&>;

private:
    DECLARE_LOGGER("HeartBeatLockService");

    void
    finish_thread();

    // Will call std::unexpected when the callback throws an exception
    void
    do_callback(const std::string& reason);

    void
    unlock(const std::string& reason);

    // Returns the time another contender has to wait before trying to grab the lock.
    boost::posix_time::time_duration
    get_session_wait_time() const;

    boost::mutex heartbeat_thread_mutex_;
    std::unique_ptr<boost::thread> heartbeat_thread_;
    GlobalLockStorePtr lock_store_;
    const boost::posix_time::time_duration update_interval_;
};

}

#endif // YT_HEARTBEAT_LOCK_SERVICE_H

// Local Variables: **
// mode: c++ **
// End: **
