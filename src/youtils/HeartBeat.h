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
