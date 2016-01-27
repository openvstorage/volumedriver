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
