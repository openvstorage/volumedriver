// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef YT_LOCK_COMMUNICATOR_H_
#define YT_LOCK_COMMUNICATOR_H_

#include "GlobalLockStore.h"
#include "HeartBeatLock.h"
#include "GlobalLockTag.h"
#include "HeartBeatLock.h"
#include "Logging.h"

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
    GlobalLockTag tag_;
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
