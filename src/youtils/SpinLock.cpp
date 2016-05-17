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
