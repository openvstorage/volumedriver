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
