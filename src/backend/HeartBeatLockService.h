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

#ifndef BACKEND_GLOBAL_LOCK_SERVICE_H
#define BACKEND_GLOBAL_LOCK_SERVICE_H

#include "GlobalLockStore.h"

#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <youtils/GlobalLockService.h>
#include <youtils/Logging.h>
#include <youtils/TimeDurationType.h>
#include <youtils/UUID.h>
#include <youtils/WithGlobalLock.h>

namespace backend
{

DECLARE_DURATION_TYPE(UpdateInterval)

class HeartBeatLockService
    : public youtils::GlobalLockService
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

    // Hopefully one day!
    // template<youtils::ExceptionPolicy policy,
    //          typename Callable,
    //          std::string(Callable::*info_member_function)() = &Callable::info()>
    // using WithGlobalLockType = youtils::WithGlobalLock<policy,
    //                                                    Callable,
    //                                                    info_member_function,
    //                                                    HeartBeatLockService,
    //                                                    BackendConnectionManagerPtr cm,
    //                                                    const boost::posix_time::time_duration& session_timeout,
    //                                                    const std::string& ns,
    //                                                    const boost::posix_time::time_duration& interrupt_timeout>;

    template<youtils::ExceptionPolicy policy,
             typename Callable,
             std::string(Callable::*info_member_function)() = &Callable::info>
    struct WithGlobalLock
    {
        using type_ = youtils::WithGlobalLock<policy,
                                              Callable,
                                              info_member_function,
                                              HeartBeatLockService,
                                              GlobalLockStorePtr,
                                              const UpdateInterval&>;
    };

private:
    DECLARE_LOGGER("BackendHeartBeatLockService");

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

#endif // BACKEND_GLOBAL_LOCK_SERVICE_H

// Local Variables: **
// mode: c++ **
// End: **
