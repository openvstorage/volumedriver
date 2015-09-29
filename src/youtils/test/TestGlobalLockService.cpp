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
