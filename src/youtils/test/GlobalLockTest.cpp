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

#include "../TestBase.h"
#include "TestGlobalLockService.h"
#include "UnlockingGlobalLockService.h"
#include "DenyLockService.h"
#include "../WithGlobalLock.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "../SourceOfUncertainty.h"
namespace youtilstest
{
using namespace youtils;

class GlobalLockTest : public TestBase
{
public:
    GlobalLockTest()
    {}

    virtual void
    SetUp()
    {
        // test_global_lock_service.reset(new TestGlobalLockService());
        // unlocking_global_lock_service.reset(new UnlockingGlobalLockService());
        // deny_locking_service.reset(new DenyLockService());

    }

    virtual void
    TearDown()
    {
        // test_global_lock_service.reset(0);
        // unlocking_global_lock_service.reset(0);
        // deny_locking_service.reset(0);
    }

    // GlobalLockService*
    // getTestGlobalLockService()
    // {
    //     return test_global_lock_service.get();
    // }

    // GlobalLockService*
    // getUnlockingGlobalLockService()
    // {
    //     return unlocking_global_lock_service.get();
    // }

    // GlobalLockService*
    // getDenyLockService()
    // {
    //     return deny_locking_service.get();
    // }

    // void
    // setUnlockingGlobalLockServiceTimeout(const boost::posix_time::time_duration& duration)
    // {
    //     unlocking_global_lock_service->timeout(duration);
    // }



// private:
//     std::unique_ptr<TestGlobalLockService> test_global_lock_service;
//     std::unique_ptr<UnlockingGlobalLockService> unlocking_global_lock_service;
//     std::unique_ptr<DenyLockService> deny_locking_service;

};


namespace
{
struct SleepingCallable : public youtils::GlobalLockedCallable
{
    SleepingCallable()
    {}

    virtual ~SleepingCallable()
    {}

    std::string
    info()
    {
        return std::string("SleepingCallable");
    }

    void
    operator()()
    {
        boost::this_thread::sleep(boost::posix_time::seconds(30));
    }


    virtual const youtils::GracePeriod&
    grace_period() const
    {
        static const youtils::GracePeriod grace_period_(boost::posix_time::seconds(1));
        return grace_period_;
    }


};
}


TEST_F(GlobalLockTest, test_unlocking)
{
    {
        using CallableT =
            UnlockingGlobalLockService::WithGlobalLock<youtils::ExceptionPolicy::ThrowExceptions,
                                                       SleepingCallable,
                                                       &SleepingCallable::info>;

        SleepingCallable sleeper;


        CallableT locked_sleeper(boost::ref(sleeper),
                                 CallableT::retry_connection_times_default(),
                                 CallableT::connection_retry_timeout_default(),
                                 "a_namespace");

        ASSERT_THROW(locked_sleeper(),
                     WithGlobalLockExceptions::LostLockException);
    }
    {
        using CallableT =
            UnlockingGlobalLockService::WithGlobalLock<youtils::ExceptionPolicy::DisableExceptions,
                                                       SleepingCallable,
                                                       &SleepingCallable::info>;

        SleepingCallable sleeper;

        CallableT locked_sleeper(boost::ref(sleeper),
                                 CallableT::retry_connection_times_default(),
                                 CallableT::connection_retry_timeout_default(),
                                 "a_namespace");

        ASSERT_NO_THROW(locked_sleeper());
        std::exception_ptr no_exception;
        ASSERT(locked_sleeper.exit_exception != no_exception);
        ASSERT_THROW(std::rethrow_exception(locked_sleeper.exit_exception),
                     WithGlobalLockExceptions::LostLockException);
    }
}

namespace
{
class SharedMemCallable
    : public youtils::GlobalLockedCallable
{
public:
    SharedMemCallable(const uint64_t id,
                      const key_t key,
                      const uint64_t max_id)
        : id_(id)
        , key_(key)
        , max_id_(max_id)
        , grace_period_(boost::posix_time::seconds(1))
    {}

    virtual ~SharedMemCallable()
    {}

    std::string
    info()
    {
        return std::string("SharedMemCallable");
    }
    void
    operator()()
    {
        int shmid = shmget(key_, 2*sizeof(uint64_t), 0666);
        ASSERT_TRUE(shmid >= 0) << "no shmid";
        uint64_t* p  = reinterpret_cast<uint64_t*>(shmat(shmid, NULL, 0));
        ASSERT_TRUE(p != (uint64_t*) -1) << "no shmat";
        LOG_INFO(id_ << ": Value of p[0] " << p[0]);
        EXPECT_EQ(0ULL, p[0]);
        uint64_t b = p[1];
        LOG_INFO(id_ << ": Value of b " << b);
        EXPECT_TRUE(b < max_id_) << "b is " << b << " not smaller than " << max_id_;
        p[0] = 1;
        usleep(10000);
        p[1] = b + 1;
        p[0] = 0;

    }

    DECLARE_LOGGER("SharedMemCallable");

    virtual const youtils::GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

private:
    const uint64_t id_;
    const key_t key_;
    const uint64_t max_id_;
    const youtils::GracePeriod grace_period_;

};

}

TEST_F(GlobalLockTest, test_mutual_exclusion)
{
    using CallableT = TestGlobalLockService::WithGlobalLock<youtils::ExceptionPolicy::DisableExceptions,
                                                            SharedMemCallable,
                                                            &SharedMemCallable::info>;

    const uint64_t max_test = 32;
    SourceOfUncertainty soc;

    key_t ipc_id = 0;
    int shmid = -1;

    do {
        ipc_id = soc(1024, 8192);
        shmid = shmget(ipc_id, 2*sizeof(uint64_t), IPC_CREAT | IPC_EXCL | 0666);
    }
    while(shmid == -1 and errno == EEXIST);

    ASSERT_TRUE(shmid >= 0);
    uint64_t* p = reinterpret_cast<uint64_t*>(shmat(shmid, NULL, 0));
    ASSERT_TRUE(p != (uint64_t*) -1);
    volatile uint64_t& p0 = p[0];
    p0 = 0;
    volatile uint64_t& p1 = p[1];
    p1 = 0;

    std::vector<boost::thread*> threads(max_test);
    std::vector<CallableT*> locked_callables(max_test);
    std::vector<SharedMemCallable*> callables(max_test);

    for(unsigned i = 0; i < max_test; ++i)
    {
        callables[i] = new SharedMemCallable(i,
                                             ipc_id,
                                             max_test);
        locked_callables[i] = new CallableT(boost::ref(*callables[i]),
                                            CallableT::retry_connection_times_default(),
                                            CallableT::connection_retry_timeout_default(),
                                            "a_namespace");

        threads[i] = new boost::thread(boost::ref(*locked_callables[i]));
    }
    for(unsigned i = 0; i < max_test; ++i)
    {
        threads[i]->join();
        delete threads[i];
        delete locked_callables[i];
        delete callables[i];
    }

    EXPECT_EQ(0ULL, p0);
    EXPECT_EQ(p1, max_test);
}
namespace
{
class ThrowingCallableException
{};

class ThrowingCallable : public youtils::GlobalLockedCallable
{
public:
    ThrowingCallable()

    {}

    std::string
    info()
    {
        return std::string("ThrowingCallable");
    }

    void
    operator()()
    {
        throw ThrowingCallableException();
    }

    virtual const youtils::GracePeriod&
    grace_period() const
    {
        static const youtils::GracePeriod grace_period_(boost::posix_time::seconds(10));
        return grace_period_;
    }


};
}


TEST_F(GlobalLockTest, test_rethrowing_exceptions)
{
    using CallableT =
        UnlockingGlobalLockService::WithGlobalLock<youtils::ExceptionPolicy::ThrowExceptions,
                                                   ThrowingCallable,
                                                   &ThrowingCallable::info>;

    ThrowingCallable throwing_callable;

    CallableT locked_throwing_callable(boost::ref(throwing_callable),
                                       CallableT::retry_connection_times_default(),
                                       CallableT::connection_retry_timeout_default(),
                                       "a_namespace");
    try
    {
        locked_throwing_callable();
    }
    catch(WithGlobalLockExceptions::CallableException& e)
    {
        try
        {
            std::rethrow_exception(e.exception_);
        }
        catch(ThrowingCallableException& e)
        {
            // All is good in the state of Denmark
        }
        catch(...)
        {
            ASSERT_TRUE(0) << "Wrong type of exception rethrown";
        }

    }

    catch(...)
    {
        ASSERT_TRUE(0) << "Wrong type of exception thrown";

    }
}

namespace
{
class LazyCallable : public youtils::GlobalLockedCallable
{
public:
    LazyCallable()
    {}

    std::string
    info()
    {
        return std::string("LazyCallable");
    }

    void
    operator()()
    {}

    virtual const youtils::GracePeriod&
    grace_period() const
    {
        static const youtils::GracePeriod grace_period_(boost::posix_time::seconds(1000));
        return grace_period_;
    }



};

}

TEST_F(GlobalLockTest, test_retrying_lock)
{
    using CallableT =
        DenyLockService::WithGlobalLock<youtils::ExceptionPolicy::ThrowExceptions,
                                        LazyCallable,
                                        &LazyCallable::info>;

    LazyCallable lazy_callable;
    // DenyLockService deny_service;

    CallableT locked_lazy_callable(boost::ref(lazy_callable),
                                   NumberOfRetries(10),
                                   CallableT::connection_retry_timeout_default());

    ASSERT_THROW(locked_lazy_callable(),
                 WithGlobalLockExceptions::UnableToGetLockException);

};

}

// Local Variables: **
// mode: c++ **
// End: **
