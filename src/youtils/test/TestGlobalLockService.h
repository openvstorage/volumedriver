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
    using WithGlobalLock = youtils::WithGlobalLock<policy,
                                                   Callable,
                                                   info_member_function,
                                                   TestGlobalLockService,
                                                   const std::string&>;

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
