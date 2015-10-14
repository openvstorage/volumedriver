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

#ifndef BACKEND_LOCK_COMMUNICATOR_H_
#define BACKEND_LOCK_COMMUNICATOR_H_

#include "GlobalLockStore.h"
#include "Lock.h"
#include "LockTag.h"

#include <boost/chrono.hpp>
#include <boost/chrono/system_clocks.hpp>

#include <youtils/Catchers.h>
#include <youtils/Logging.h>

namespace backend
{

class LockCommunicator
{
public:
    LockCommunicator(GlobalLockStorePtr lock_store,
                     const boost::posix_time::time_duration connection_timeout,
                     const boost::posix_time::time_duration interrupt_timeout);

    using MilliSeconds = boost::chrono::milliseconds;
    using Clock = boost::chrono::steady_clock;

    bool
    lock_exists();

    void
    freeLock();

    Lock
    getLock();

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
    DECLARE_LOGGER("BackendLockCommunicator");

    GlobalLockStorePtr lock_store_;
    LockTag tag_;
    Lock lock_;

    // Don't try to update more than this number of times...
    // apparantly a conforming application can otherwise put this thing in an
    // infinite loop.
    static const uint64_t max_update_retries = 100;
};

}

#endif // BACKEND_LOCK_COMMUNICATOR_H_

// Local Variables: **
// mode: c++ **
// End: **
