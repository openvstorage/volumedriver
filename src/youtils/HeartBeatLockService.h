// Copyright (C) 2016 iNuron NV
//
// This file is part of Open vStorage Open Source Edition (OSE),
// as available from
//
//      http://www.openvstorage.org and
//      http://www.openvstorage.com.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
// as published by the Free Software Foundation, in version 3 as it comes in
// the LICENSE.txt file of the Open vStorage OSE distribution.
// Open vStorage is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY of any kind.

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
