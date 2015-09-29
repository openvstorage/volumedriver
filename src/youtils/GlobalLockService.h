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

#ifndef GLOBAL_LOCK_SERVICE_H
#define GLOBAL_LOCK_SERVICE_H

#include "GlobalLockedCallable.h"
#include "IOException.h"
#include "TimeDurationType.h"

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace youtils
{

class GlobalLockService
{
public:
    MAKE_EXCEPTION(GlobalLockServiceException, fungi::IOException);
    MAKE_EXCEPTION(InsufficientConnectionDetails, GlobalLockServiceException);
    MAKE_EXCEPTION(NoSuchLock, GlobalLockServiceException);

    // Nobody gets the lock before your lost_lock_callback has been called and it has returned...
    typedef void (*lost_lock_callback)(void* data,
                                       const std::string& reason);

    GlobalLockService(const GracePeriod& grace_period,
                      lost_lock_callback callback = 0,
                      void* data = 0)
        : callback_(callback),
          data_(data),
          grace_period_(grace_period)
    {}

    virtual ~GlobalLockService() = 0;

    virtual bool
    lock() = 0;

    virtual void
    unlock() = 0;

protected:
    lost_lock_callback callback_;
    void* data_;
    boost::posix_time::time_duration grace_period_;
};

}

#endif // GLOBAL_LOCK_SERVICE_H

// Local Variables: **
// mode: c++ **
// End: **
