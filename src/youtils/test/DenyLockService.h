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

#ifndef DENY_LOCK_SERVICE_H_
#define DENY_LOCK_SERVICE_H_

#include "../GlobalLockService.h"
#include "../WithGlobalLock.h"

namespace youtilstest
{
class DenyLockService : public youtils::GlobalLockService
{
public:
    DenyLockService(const youtils::GracePeriod& grace_period,
                    lost_lock_callback callback,
                     void* data)
        :GlobalLockService(grace_period,
                           callback,
                           data)
    {}


    template<youtils::ExceptionPolicy policy,
             typename Callable,
             std::string(Callable::*info_member_function)() = &Callable::info()>
    struct WithGlobalLock
    {
        typedef youtils::WithGlobalLock<policy,
                                        Callable,
                                        info_member_function,
                                        DenyLockService> type_;
    };

    virtual bool
    lock()
    {
        return false;
    }

    virtual void
    unlock()
    {};

};
}
#endif // DENY_LOCK_SERVICE_H_

// Local Variables: **
// bvirtual-targets: ("target/bin/youtils_test") **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
