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

#include "ExGTest.h"

#include <algorithm>
#include <iostream>

#include <boost/filesystem/fstream.hpp>

#include <youtils/TestBase.h>
#include <boost/thread.hpp>
#include <boost/thread/reverse_lock.hpp>
namespace volumedrivertest
{

namespace fs = boost::filesystem;

struct LockedAccess
{
public:
    typedef boost::unique_lock<boost::mutex> unique_lock_t;

    LockedAccess()
        : inside(false)
        , total_calls_(0)
        , return_from_call(false)
    {}

    unique_lock_t
    get_unique_lock()
    {
        return unique_lock_t(m_, boost::defer_lock);
    }

    void
    protected_call(unique_lock_t* lock = nullptr)
    {
        std::unique_ptr<unique_lock_t> _lock_;

        if(not lock)
        {
            _lock_.reset(new unique_lock_t(m_,boost::defer_lock));
            lock = _lock_.get();
        }

        if(not lock->owns_lock())
        {
            bool res = lock->try_lock();
            if(not res)
            {
                throw 1;
            }
        }
        if(lock->mutex() != &m_)
        {
            throw 2;
        }
        if(inside)
        {
            throw 3;

        }

        inside = true;
        total_calls_++;
        while(not return_from_call)
        {
            usleep(100000);
        }

        inside = false;
    }

    void
    unlocking_call(unique_lock_t* lock = nullptr,
                   bool hang = true)
    {
        std::unique_ptr<unique_lock_t> _lock_;

        if(not lock)
        {
            _lock_.reset(new unique_lock_t(m_,boost::defer_lock));
            lock = _lock_.get();
        }

        if(not lock->owns_lock())
        {
            bool res = lock->try_lock();
            if(not res)
            {
                throw 1;
            }
        }
        if(lock->mutex() != &m_)
        {
            throw 2;
        }
        {
            boost::reverse_lock<unique_lock_t> l(*lock);
            inside = true;
            if(hang)
            {
                while(not return_from_call)
                {
                    usleep(100000);
                }
            }

        }
        total_calls_++;
        inside = false;
    }


    unsigned
    total_calls() const
    {
        return total_calls_;
    }


    boost::mutex m_;
    boost::atomic<bool> inside;
    unsigned total_calls_;
    boost::atomic<bool> return_from_call;
};



struct ProtectedCallInThread
{
    ProtectedCallInThread(LockedAccess& la)
        : la_(la)
    {}

    void
    operator()(LockedAccess::unique_lock_t* lock = 0)
    {
        la_.protected_call(lock);
    }
    LockedAccess& la_;

};

struct UnlockingCallInThread
{
    UnlockingCallInThread(LockedAccess& la)
        : la_(la)
    {}

    void
    operator()(LockedAccess::unique_lock_t* lock = 0)
    {
        la_.unlocking_call(lock);
    }
    LockedAccess& la_;

};



class LockingTest
    : public youtilstest::TestBase
{

};

TEST_F(LockingTest, test1)
{
    LockedAccess la;
    ProtectedCallInThread l(la);

    boost::thread t(l);
    while(not la.inside)
    {
        usleep(100000);
    }

    EXPECT_THROW(la.protected_call(),
                 int);

    LockedAccess::unique_lock_t lock = la.get_unique_lock();

    EXPECT_THROW(la.protected_call(&lock),
                 int);

    EXPECT_FALSE(lock.try_lock());


    la.return_from_call = true;
    t.join();

    EXPECT_NO_THROW(la.protected_call());

    EXPECT_NO_THROW(la.protected_call(&lock));
    // YOU GET THE LOCK BACK LOCKED HERE!!
    EXPECT_TRUE(lock.owns_lock());
    EXPECT_NO_THROW(la.protected_call(&lock));

    EXPECT_EQ(la.total_calls(), 4);

}

TEST_F(LockingTest, test2)
{
    LockedAccess la;
    UnlockingCallInThread l(la);

    boost::thread t(l);
    while(not la.inside)
    {
        usleep(100000);
    }

    EXPECT_NO_THROW(la.unlocking_call(nullptr,
                                      false));

    {
        {

            LockedAccess::unique_lock_t lock = la.get_unique_lock();

            EXPECT_NO_THROW(la.unlocking_call(&lock,
                                              false));
            // YOU GET THE LOCK BACK LOCKED HERE!!
            EXPECT_TRUE(lock.owns_lock());
            EXPECT_NO_THROW(la.unlocking_call(&lock,
                                              false));


            la.return_from_call = true;
        }

        t.join();
    }

    {

        LockedAccess::unique_lock_t lock = la.get_unique_lock();
        EXPECT_NO_THROW(la.unlocking_call());

        EXPECT_NO_THROW(la.unlocking_call(&lock));
        // YOU GET THE LOCK BACK LOCKED HERE!!
        EXPECT_TRUE(lock.owns_lock());
        EXPECT_NO_THROW(la.unlocking_call(&lock));
    }

    EXPECT_EQ(la.total_calls(), 7);

}

}
