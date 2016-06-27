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

#ifndef _CONDVAR_H
#define	_CONDVAR_H

#include <pthread.h>


namespace fungi {
class Mutex;

class CondVar
{
public:
    explicit CondVar(Mutex &mutex);

    ~CondVar();

    // return false if the wait was interrupted
    // true if the wait timed out
    // mutex_ needs to be locked by the caller
    bool
    wait_sec_no_lock(int sec);

    bool
    wait_nsec_no_lock(uint64_t nsecs);

    // This has potential for a race with its signal*() counterpart because
    // it grabs the mutex_ too late.
    // Use wait_sec_no_lock() instead - this expects the mutex_ to be
    // locked when called!
    bool wait_sec(int sec);

    //void wait(); // not used normally

    // mutex_ needs to be locked by the caller
    void wait_no_lock();

    // usually if you use signal like this isof the
    // no_lock variant this has big potential to introduce a race
    // condition; thats why it is deprecated!
    // if you really need it, use lock_and_signal()
    void signal() __attribute__ ((deprecated));


    void signal_no_lock();

    void lock_and_signal();

private:
    CondVar();
    CondVar(const CondVar &);
    CondVar &operator=(const CondVar &);
    Mutex &mutex_;
    pthread_cond_t cond_;
};
}


#endif	/* _CONDVAR_H */


// Local Variables: **
// mode: c++ **
// End: **
