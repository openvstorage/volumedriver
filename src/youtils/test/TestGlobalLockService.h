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

#ifndef TEST_GLOBAL_LOCK_SERVICE_H
#define TEST_GLOBAL_LOCK_SERVICE_H

#include <boost/thread.hpp>
#include <set>
#include "../GlobalLockService.h"
#include "../WithGlobalLock.h"
namespace youtilstest
{

class TestGlobalLockService : public youtils::GlobalLockService
{
public:
    TestGlobalLockService(const youtils::GracePeriod& /*grace_period*/,
                          lost_lock_callback callback,
                          void* data,
                          const std::string& ns)
        : GlobalLockService(youtils::GracePeriod(boost::posix_time::seconds(1)),
                            callback,
                            data)
        , ns_(ns)
    {}

    virtual ~TestGlobalLockService();

    template<youtils::ExceptionPolicy policy,
             typename Callable,
             std::string(Callable::*info_member_function)() = &Callable::info()>
    struct WithGlobalLock
    {

        typedef youtils::WithGlobalLock<policy,
                                        Callable,
                                        info_member_function,
                                        TestGlobalLockService,
                                        const std::string&> type_;
    };

    virtual bool
    lock();

    virtual void
    unlock();

private:

    static boost::mutex mutex_;
    const std::string ns_;

    static std::map<std::string, std::pair<lost_lock_callback, void*> > locks;
    typedef std::map<std::string, std::pair<lost_lock_callback, void*> >::const_iterator const_lock_iterator;
    typedef std::map<std::string, std::pair<lost_lock_callback, void*> >::iterator lock_iterator;
};

}
#endif // TEST_GLOBAL_LOCK_SERVICE_H

// Local Variables: **
// bvirtual-targets: ("bin/backup_volume_driver") **
// compile-command: "scons -D --kernel_version=system  --ignore-buildinfo -j 4" **
// mode: c++ **
// End: **
