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

#ifndef YT_HEARTBEAT_H_
#define YT_HEARTBEAT_H_

#include "GlobalLockStore.h"
#include "HeartBeatLockCommunicator.h"
#include "Logging.h"

namespace youtils
{

class HeartBeat
{
public:
    using FinishThreadFun = std::function<void()>;
    using TimeDuration = boost::posix_time::time_duration;

    // Careful these are copied *by value* before the thread sees them!
    HeartBeat(GlobalLockStorePtr lock_store,
              FinishThreadFun finish_thread_fun,
              const TimeDuration& heartbeat_timeout,
              const TimeDuration& interrupt_timeout);

    const std::string&
    name() const;

    void
    operator()();

    bool
    grab_lock();

private:
    DECLARE_LOGGER("HeartBeat");

    FinishThreadFun finish_thread_fun_;
    // Only instantiate the communicator when the lock is grabbed??
    HeartBeatLockCommunicator lock_communicator_;
    const TimeDuration heartbeat_timeout_;
};

}

#endif // YT_HEARTBEAT_H_

// Local Variables: **
// mode: c++ **
// End: **
