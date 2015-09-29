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

#ifndef SPIN_LOCK_H_
#define SPIN_LOCK_H_

#include <string>
#include <pthread.h>
#include "Logging.h"


namespace fungi
{

class SpinLock

{
public:
    SpinLock();

    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;


    ~SpinLock();

    void
    lock();

    bool
    tryLock();

    void
    unlock();

    void
    assertLocked();

private:
    DECLARE_LOGGER("SpinLock");

    pthread_spinlock_t lock_;
};

class ScopedSpinLock
{
public:
    explicit ScopedSpinLock(SpinLock& sl);

    ScopedSpinLock(const ScopedSpinLock&) = delete;
    ScopedSpinLock& operator=(const ScopedSpinLock&) = delete;

    ~ScopedSpinLock();


private:
    DECLARE_LOGGER("ScopedSpinLock");

    SpinLock& sl_;
};

}

#endif // !SPIN_LOCK_H_

// Local Variables: **
// mode: c++ **
// End: **
