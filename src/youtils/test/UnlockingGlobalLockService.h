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
    using WithGlobalLock = youtils::WithGlobalLock<policy,
                                                   Callable,
                                                   info_member_function,
                                                   UnlockingGlobalLockService,
                                                   const std::string&>;

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
