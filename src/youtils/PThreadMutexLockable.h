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
