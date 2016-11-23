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

#include "Assert.h"
#include "IOException.h"
#include "RWLock.h"

#include <string.h>

#include <boost/optional.hpp>

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
