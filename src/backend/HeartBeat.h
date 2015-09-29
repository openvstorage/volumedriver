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

#ifndef BACKEND_HEARTBEAT_H_
#define BACKEND_HEARTBEAT_H_

#include "LockCommunicator.h"

#include <youtils/GlobalLockService.h>
#include <youtils/Logging.h>

namespace backend
{

class GlobalLockService;

class HeartBeat
{
public:
    typedef youtils::GlobalLockService::lost_lock_callback lost_lock_callback;

    // Carefull these are copied *by value* before the thread sees them!
    HeartBeat(GlobalLockService& rest_global_lock_service,
              const boost::posix_time::time_duration& heartbeat_timeout,
              const boost::posix_time::time_duration& interrupt_timeout);

    void
    operator()();

    bool
    grab_lock();

private:
    DECLARE_LOGGER("BackendHeartBeat");

    GlobalLockService& global_lock_service_;
    // Only instantiate the communicator when the lock is grabbed??
    LockCommunicator lock_communicator_;
    const boost::posix_time::time_duration heartbeat_timeout_;
};

}

#endif // BACKEND_HEARTBEAT_H_

// Local Variables: **
// mode: c++ **
// End: **
