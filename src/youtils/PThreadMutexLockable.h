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

#ifndef YOUTILS_PTHREAD_MUTEX_LOCKABLE_H_
#define YOUTILS_PTHREAD_MUTEX_LOCKABLE_H_

#include <stdexcept>
#include <system_error>

#include <pthread.h>

// Implements boost::thread's lockable concept.
namespace youtils
{

class PThreadMutexLockable
{
public:
    explicit PThreadMutexLockable(pthread_mutex_t& m)
        : m_(m)
    {}

    ~PThreadMutexLockable() = default;

    PThreadMutexLockable(const PThreadMutexLockable&) = default;

    PThreadMutexLockable&
    operator=(const PThreadMutexLockable&) = default;

    void
    lock()
    {
        maybe_throw_(::pthread_mutex_lock(&m_));
    }

    void
    unlock()
    {
        maybe_throw_(::pthread_mutex_unlock(&m_));
    }

    bool
    try_lock()
    {
        int ret = ::pthread_mutex_trylock(&m_);
        if (ret == 0)
        {
            return true;
        }
        else if (ret == EBUSY)
        {
            return false;
        }
        else
        {
            maybe_throw_(ret);
        }

        UNREACHABLE;
    }

private:
    pthread_mutex_t& m_;

    inline void
    maybe_throw_(int error)
    {
        if (error != 0)
        {
            throw std::system_error(std::error_code(error,
                                                    std::system_category()));
        }
    }
};

}

#endif //! YOUTILS_PTHREAD_MUTEX_LOCKABLE_H_
