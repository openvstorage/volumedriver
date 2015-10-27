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

#include <cstring>
#include <cerrno>

#include "SpinLock.h"
#include "Assert.h"
#include "IOException.h"

namespace fungi {

SpinLock::SpinLock()
{
    int ret = ::pthread_spin_init(&lock_,
                                  PTHREAD_PROCESS_PRIVATE);
    if (ret)
    {
        LOG_ERROR("Failed to initialize spinlock " << &lock_ << ": " <<
                  strerror(ret));
        throw fungi::IOException("Failed to initialize spinlock");
    }
}

SpinLock::~SpinLock()
{
#ifndef NDEBUG
    VERIFY(tryLock());
    unlock();
#endif
    int ret = pthread_spin_destroy(&lock_);
    if (ret)
    {
        LOG_ERROR("Failed to destroy spinlock " << &lock_ << ": " <<
                  strerror(ret));
    }
}

void
SpinLock::lock()
{
    int ret = pthread_spin_lock(&lock_);
    if (ret)
    {
        LOG_ERROR("Failed to lock spinlock " << &lock_ << ": " << strerror(ret));
        throw fungi::IOException("Failed to lock spinlock");
    }
}

bool
SpinLock::tryLock()
{
    int ret = pthread_spin_trylock(&lock_);
    switch (ret) {
    case 0:
        return true;
    case EBUSY:
        return false;
    default:
        LOG_ERROR("Failed to trylock spinlock " << &lock_ << ": " <<
                  strerror(ret));
        throw fungi::IOException("Failed to trylock spinlock");
    }
}

void
SpinLock::assertLocked()
{
    VERIFY(!tryLock());
}

void
SpinLock::unlock()
{
    int ret = pthread_spin_unlock(&lock_);
    if (ret)
    {
        LOG_ERROR("Failed to unlock spinlock " << &lock_ << ": " << strerror(ret));
        throw fungi::IOException("Failed to unlock spinlock");
    }
}

ScopedSpinLock::ScopedSpinLock(SpinLock& sl)
    : sl_(sl)
{
    sl_.lock();
}

ScopedSpinLock::~ScopedSpinLock()
{
    sl_.unlock();
}

}

// Local Variables: **
// mode: c++ **
// End: **
