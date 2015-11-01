// Copyright 2015 iNuron NV
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

#ifndef NO_GLOBAL_LOCKING_SERVICE_H
#define NO_GLOBAL_LOCKING_SERVICE_H

#include "GlobalLockService.h"
namespace youtils
{
class NoGlobalLockingService : public GlobalLockService
{
public:
    NoGlobalLockingService(const GracePeriod& grace_period)
        : GlobalLockService(grace_period)
    {}

    ~NoGlobalLockingService()
    {}

    virtual bool
    lock()
    {
        return true;
    }

    virtual void
    unlock()
    {}
};

}

#endif // NO_GLOBAL_LOCKING_SERVICE_H

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// End: **
