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

#ifndef YT_LOCK_COMMUNICATOR_H_
#define YT_LOCK_COMMUNICATOR_H_

#include "GlobalLockStore.h"
#include "HeartBeatLock.h"
#include "HeartBeatLock.h"
#include "Logging.h"
#include "UniqueObjectTag.h"

#include <boost/chrono.hpp>
#include <boost/chrono/system_clocks.hpp>
#include <boost/thread.hpp>

namespace youtils
{

class HeartBeatLockCommunicator
{
public:
    HeartBeatLockCommunicator(GlobalLockStorePtr lock_store,
                              const boost::posix_time::time_duration connection_timeout,
                              const boost::posix_time::time_duration interrupt_timeout);

    using MilliSeconds = boost::chrono::milliseconds;
    using Clock = boost::chrono::steady_clock;

    bool
    lock_exists();

    void
    freeLock();

    bool
    overwriteLock();

    bool
    putLock();

    bool
    tryToAcquireLock();

    bool
    refreshLock(MilliSeconds max_wait_time);

    const std::string&
    name() const;

private:
    DECLARE_LOGGER("HeartBeatLockCommunicator");

    GlobalLockStorePtr lock_store_;
    std::unique_ptr<UniqueObjectTag> tag_;
    HeartBeatLock lock_;

    // Don't try to update more than this number of times...
    // apparantly a conforming application can otherwise put this thing in an
    // infinite loop.
    static const uint64_t max_update_retries = 100;

    HeartBeatLock
    getLock();
};

}

#endif // BACKEND_LOCK_COMMUNICATOR_H_

// Local Variables: **
// mode: c++ **
// End: **
