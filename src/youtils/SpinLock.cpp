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
