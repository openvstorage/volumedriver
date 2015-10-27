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

#ifndef UNLOCKING_GLOBAL_LOCK_SERVICE_H_
#define UNLOCKING_GLOBAL_LOCK_SERVICE_H_
#include "../GlobalLockService.h"
#include <boost/thread.hpp>
#include "../WithGlobalLock.h"

namespace youtilstest
{
using namespace youtils;
class UnlockingGlobalLockService : public GlobalLockService
{
    class UnlockCallback
    {
    public:
        UnlockCallback(const std::string& lock_name,
                       lost_lock_callback callback,
                       void* data,
                       UnlockingGlobalLockService& service)
            : lock_name_(lock_name)
            , callback_(callback)
            , data_(data)
            , service_(service)
        {}

        void
        operator()();
    private:
        const std::string lock_name_;
        GlobalLockService::lost_lock_callback callback_;
        void* data_;
        UnlockingGlobalLockService& service_;
    };

    friend class UnlockCallback;

public:
    UnlockingGlobalLockService(const GracePeriod& grace_period,
                               lost_lock_callback callback,
                               void* data,
                               const std::string& ns,
                               const boost::posix_time::time_duration& unlock_timeout = boost::posix_time::seconds(10));

    template<youtils::ExceptionPolicy policy,
             typename Callable,
             std::string(Callable::*info_member_function)() = &Callable::info()>
    struct WithGlobalLock
    {
        typedef youtils::WithGlobalLock<policy,
                                        Callable,
                                        info_member_function,
                                        UnlockingGlobalLockService,
                                        const std::string&> type_;
    };


    virtual ~UnlockingGlobalLockService();

    virtual bool
    lock();

    virtual void
    unlock();

    DECLARE_LOGGER("UnlockingGlobalLockService");

    void
    operator()();

    boost::posix_time::time_duration
    timeout(const boost::posix_time::time_duration duration)
    {
        boost::posix_time::time_duration res = unlock_timeout_;
        unlock_timeout_ = duration;
        return res;
    }
private:
    const std::string ns_;

    boost::posix_time::time_duration unlock_timeout_;

    static boost::mutex unlocking_threads_mutex_;
    // std::list<boost::thread*> unlocking_threads_;
    static std::map<std::string, boost::thread* > unlocking_threads_;

    static boost::mutex joinable_threads_mutex_;

    static boost::condition_variable joinable_threads_condition_variable_;

    static std::list<boost::thread*> joinable_threads_;
    static std::unique_ptr<boost::thread> thread_joiner_;

    typedef std::map<std::string, boost::thread*>::const_iterator const_lock_iterator;
    typedef std::map<std::string, boost::thread*>::iterator lock_iterator;

    void
    finish_thread(const std::string& ns,
                  lost_lock_callback callback = 0,
                  void* data = 0);

};


}
#endif // UNLOCKING_GLOBAL_LOCK_SERVICE_H_

// Local Variables: **
// bvirtual-targets: ("bin/backup_volume_driver") **
// compile-command: "scons -D --kernel_version=system  --ignore-buildinfo -j 4" **
// mode: c++ **
// End: **
