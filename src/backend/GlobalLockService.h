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

#include "BackendConnectionInterface.h"
#include "BackendConnectionManager.h"

#include <map>

#include <boost/thread.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

#include <youtils/GlobalLockService.h>
#include <youtils/System.h>
#include "youtils/TimeDurationType.h"
#include <youtils/UUID.h>
#include <youtils/WithGlobalLock.h>

namespace backend
{

class HeartBeat;
class LockCommunicator;

DECLARE_DURATION_TYPE(UpdateInterval)

class GlobalLockService
    : public youtils::GlobalLockService
{
    friend class HeartBeat;
    friend class LockCommunicator;

public:
    GlobalLockService(const youtils::GracePeriod& grace_period,
                      lost_lock_callback callback,
                      void* data,
                      BackendConnectionManagerPtr cm,
                      const UpdateInterval& update_interval,
                      const Namespace& ns);

    virtual ~GlobalLockService();

    virtual bool
    lock();

    virtual void
    unlock();

    DECLARE_LOGGER("BackendGlobalLockService");

    // Hopefully one day!
    // template<youtils::ExceptionPolicy policy,
    //          typename Callable,
    //          std::string(Callable::*info_member_function)() = &Callable::info()>
    // using WithGlobalLockType = youtils::WithGlobalLock<policy,
    //                                                    Callable,
    //                                                    info_member_function,
    //                                                    GlobalLockService,
    //                                                    BackendConnectionManagerPtr cm,
    //                                                    const boost::posix_time::time_duration& session_timeout,
    //                                                    const std::string& ns,
    //                                                    const boost::posix_time::time_duration& interrupt_timeout>;

    template<youtils::ExceptionPolicy policy,
             typename Callable,
             std::string(Callable::*info_member_function)() = &Callable::info>
    struct WithGlobalLock
    {
        typedef youtils::WithGlobalLock<policy,
                                        Callable,
                                        info_member_function,
                                        GlobalLockService,
                                        BackendConnectionManagerPtr,
                                        const UpdateInterval&,
                                        const Namespace&> type_;
    };

private:
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
    BackendConnectionManagerPtr cm_;
    const Namespace ns_;

    const boost::posix_time::time_duration update_interval_;
};

}

#endif // BACKEND_GLOBAL_LOCK_SERVICE_H

// Local Variables: **
// mode: c++ **
// End: **
