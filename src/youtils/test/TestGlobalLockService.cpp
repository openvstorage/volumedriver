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

#include "TestGlobalLockService.h"

namespace youtilstest
{

std::map<std::string, std::pair<youtils::GlobalLockService::lost_lock_callback, void*> >
TestGlobalLockService::locks;

boost::mutex
TestGlobalLockService::mutex_;


TestGlobalLockService::~TestGlobalLockService()
{}

bool
TestGlobalLockService::lock()
{
    boost::unique_lock<boost::mutex> lock(mutex_);
    if(locks.find(ns_) != locks.end())
    {
        return false;
    }
    else
    {
        locks[ns_] = std::make_pair(callback_, data_);
        return true;
    }
}

void
TestGlobalLockService::unlock()
{

    boost::unique_lock<boost::mutex> lock(mutex_);
    lock_iterator a = locks.find(ns_);

    if(a != locks.end())
    {
        locks.erase(a);
    }
    else
    {
        throw NoSuchLock("unlock: lock was not found");
    }
}
}


// Local Variables: **
// bvirtual-targets: ("bin/backup_volume_driver") **
// compile-command: "scons -D --kernel_version=system -j 4" **
// mode: c++ **
// End: **
