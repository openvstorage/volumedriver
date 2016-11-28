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

}

#endif // !SPIN_LOCK_H_

// Local Variables: **
// mode: c++ **
// End: **
