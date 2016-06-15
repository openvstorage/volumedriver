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

#ifndef _RWLOCK_H
#define _RWLOCK_H


#include <pthread.h>
#include "Logging.h"
#include <string>
#include <cerrno>
#include <assert.h>


namespace fungi {

class RWLock

{
public:
    explicit RWLock(const std::string &name);

    RWLock(const RWLock&) = delete;
    RWLock& operator=(const RWLock&) = delete;

    ~RWLock();

    void
    writeLock();

    void
    lock()
    {
        return writeLock();
    }

    bool
    tryWriteLock();

    bool
    try_lock()
    {
        return tryWriteLock();
    }

    void
    readLock();

    void
    lock_shared()
    {
        return readLock();
    }

    bool
    tryReadLock();

    bool
    try_lock_shared()
    {
        return tryReadLock();
    }

    void
    unlock();

    void
    unlock_shared()
    {
        return unlock();
    }

    inline void
    assertWriteLocked() const
    {
        assert(pthread_rwlock_tryrdlock(&rwLock_) == EBUSY);
    }

    inline void
    assertReadLocked() const
    {
        assert(pthread_rwlock_trywrlock(&rwLock_) == EBUSY);
    }

    void
    assertLocked();

private:
    DECLARE_LOGGER("RWLock");

#ifndef NDEBUG
    mutable
#endif
    pthread_rwlock_t rwLock_;
    const std::string name_;
};

class ScopedReadLock
{
public:
    explicit ScopedReadLock(RWLock &rw);

    ScopedReadLock(const ScopedReadLock&) = delete;
    ScopedReadLock& operator=(const ScopedReadLock&) = delete;

    ~ScopedReadLock();

private:
    DECLARE_LOGGER("ScopedReadLock");

    RWLock &rw_;
};

class ScopedWriteLock
{
public:
    explicit ScopedWriteLock(RWLock &rw);

    ScopedWriteLock(const ScopedWriteLock&) = delete;
    ScopedWriteLock& operator=(const ScopedWriteLock&) = delete;


    ~ScopedWriteLock();

private:
    DECLARE_LOGGER("ScopedWriteLock");

    RWLock &rw_;
};

}

#endif // !_RWLOCK_H

// Local Variables: **
// mode: c++ **
// End: **
