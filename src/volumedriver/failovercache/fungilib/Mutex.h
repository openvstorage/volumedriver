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

#ifndef _MUTEX_H
#define    _MUTEX_H

#include <pthread.h>
#include <youtils/Logging.h>
#include <string>

namespace fungi {
    class Mutex {
    public:
        enum MutexType
        {
            RecursiveMutex,
            ErrorCheckingMutex,
            NormalMutex
        };

        DECLARE_LOGGER("Mutex");
        explicit Mutex(const std::string &name,
                       MutexType type = NormalMutex);

        ~Mutex();

        void lock();
        void unlock();

        pthread_mutex_t *handle(); // needed for CondVar

        const std::string & getName() const;

        void
        assertLocked() const;

        bool
        try_lock();

    private:
        Mutex(const Mutex &);
        Mutex &operator=(const Mutex &);

#ifndef NDEBUG
        mutable
#endif
        pthread_mutex_t mutex_;
        const std::string name_;
    };

    class ScopedLock {
    public:
        explicit ScopedLock(Mutex &m,
                            bool trylock = false);
        ~ScopedLock();
        DECLARE_LOGGER("ScopedLock");

    private:
        Mutex &m_;
        ScopedLock(const ScopedLock &);
        ScopedLock &operator=(const ScopedLock &);
    };
}


#endif    /* _MUTEX_H */

// aaa
