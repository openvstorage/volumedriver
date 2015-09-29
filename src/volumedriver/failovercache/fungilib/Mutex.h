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
