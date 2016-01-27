// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <pthread.h>
#include <string.h>

#include "IOException.h"
#include "RWLock.h"
#include "Assert.h"

namespace fungi {

RWLock::RWLock(const std::string &name)
    : name_(name)
{
    int ret = pthread_rwlock_init(&rwLock_, NULL);
    if (ret) {
        throw fungi::IOException("Failed to init rwlock", name_.c_str(), ret);
    }
}

RWLock::~RWLock()
{
    int ret = pthread_rwlock_destroy(&rwLock_);
    if (ret) {
        LOG_ERROR("Failed to destroy " << name_ << ": " << strerror(ret));
        // ugh - what now? assert(false)?
    }
}

void RWLock::writeLock()
{
    int ret = pthread_rwlock_wrlock(&rwLock_);
    if (ret) {
        throw fungi::IOException("RWLock::writeLock", name_.c_str(), ret);
    }
}

bool RWLock::tryWriteLock()
{
    int ret = pthread_rwlock_trywrlock(&rwLock_);
    switch (ret) {
    case 0:
        return true;
    case EBUSY:
        return false;
    default:
        throw fungi::IOException("RWLock::tryWriteLock", name_.c_str(), ret);
    }
}

void RWLock::assertLocked()
{
    VERIFY(!tryWriteLock());
}

void RWLock::readLock()
{
    int ret = pthread_rwlock_rdlock(&rwLock_);
    if (ret) {
        throw fungi::IOException("RWLock::readLock", name_.c_str(), ret);
    }
}

bool RWLock::tryReadLock()
{
    int ret = pthread_rwlock_tryrdlock(&rwLock_);
    switch (ret) {
    case 0:
        return true;
    case EBUSY:
        return false;
    default:
        throw fungi::IOException("RWLock::tryReadLock", name_.c_str(), ret);
    }
}

void RWLock::unlock()
{
    int ret = pthread_rwlock_unlock(&rwLock_);
    if (ret) {
        throw fungi::IOException("RWLock::unlock", name_.c_str(), ret);
    }
}


ScopedReadLock::ScopedReadLock(RWLock &rw)
    : rw_(rw)
{
    rw_.readLock();
}

ScopedReadLock::~ScopedReadLock()
{
    rw_.unlock();
}

ScopedWriteLock::ScopedWriteLock(RWLock &rw)
    : rw_(rw)
{
    rw_.writeLock();
}

ScopedWriteLock::~ScopedWriteLock()
{
    rw_.unlock();
}
}

// Local Variables: **
// mode: c++ **
// End: **
