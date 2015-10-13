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

#include "BackendTestBase.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <youtils/WithGlobalLock.h>

#include "../BackendConnectionManager.h"
#include "../BackendInterface.h"
#include "../GlobalLockService.h"
#include "../LockCommunicator.h"
#include "../LockStore.h"

namespace backendtest
{
namespace be = backend;
namespace yt = youtils;

using yt::WithGlobalLock;

namespace
{

DECLARE_LOGGER("LockTest");

}

class LockTest
    : public be::BackendTestBase
{
public:
    using LockService = be::GlobalLockService;

    LockTest()
        : be::BackendTestBase("LockTest")
    {};

    virtual void
    SetUp()
    {
        be::BackendTestBase::SetUp();
        nspace_ = make_random_namespace();
    }

    virtual void
    TearDown()
    {
        nspace_.reset();
        be::BackendTestBase::TearDown();
    }

    const be::Namespace&
    testNamespace()
    {
        return nspace_->ns();
    }

    DECLARE_LOGGER("LockTest");
    std::unique_ptr<WithRandomNamespace> nspace_;
};

struct TestCallBack
{
    TestCallBack()
        : called_(false)
    {   }


    void
    callback(const std::string&)
    {
        called_ = true;
    }
    bool called_;
};

template<typename T>
void
trampoline(void* data,
           const std::string& reason)
{
    static_cast<T*>(data)->callback(reason);
}

TEST_F(LockTest, dump_restlock)
{
    // WON'T COMPILE WITHOUT CHANGING RESTLOCK FUNCTIONS TO PUBLIC!!
    // be::rest::Lock lock(boost::posix_time::seconds(42),
    //                         boost::posix_time::seconds(10));

    // std::string str;
    // lock.save(str);
    // std::cout << std::endl << str << std::endl;
}

TEST_F(LockTest, test_no_lock_if_namespace_doesnt_exist)
{
    {
        be::BackendConnectionInterfacePtr c(cm_->getConnection());
        if (not c->hasExtendedApi())
        {
            SUCCEED() << "these tests require a backend that supports the extended api";
            return;
        }
    }

    const be::Namespace ns(yt::UUID().str());

    TestCallBack callback;

    be::GlobalLockStorePtr
        lock_store(new be::LockStore(cm_->newBackendInterface(ns)));
    LockService lock_service(yt::GracePeriod(boost::posix_time::seconds(1)),
                             &trampoline<TestCallBack>,
                             &callback,
                             lock_store,
                             be::UpdateInterval(boost::posix_time::seconds(1)));

    ASSERT_FALSE(lock_service.lock());
    EXPECT_NO_THROW(lock_service.unlock());
    sleep(1);
    EXPECT_FALSE(callback.called_);
}

TEST_F(LockTest, test_lock_if_namespace_exists)
{
    {
        be::BackendConnectionInterfacePtr c(cm_->getConnection());
        if (not c->hasExtendedApi())
        {
            SUCCEED() << "these tests require a backend that supports the extended api";
            return;
        }
    }

    const yt::GracePeriod grace_period(boost::posix_time::milliseconds(100));
    TestCallBack callback;

    be::GlobalLockStorePtr
        lock_store(new be::LockStore(cm_->newBackendInterface(testNamespace())));

    LockService lock_service(yt::GracePeriod(boost::posix_time::seconds(1)),
                             &trampoline<TestCallBack>,
                             &callback,
                             lock_store,
                             be::UpdateInterval(boost::posix_time::seconds(1)));

    ASSERT_TRUE(lock_service.lock());
    EXPECT_NO_THROW(lock_service.unlock());
    sleep(1);
    EXPECT_TRUE(callback.called_);
}

namespace
{

class SharedMemCallable
    : public yt::GlobalLockedCallable
{
public:
    SharedMemCallable(const uint64_t id,
                      const key_t key,
                      const uint64_t max_id,
                      const yt::GracePeriod grace_period)
        : id_(id)
        , key_(key)
        , max_id_(max_id)
        , grace_period_(grace_period)
    {}

    std::string
    info()
    {
        return std::string("SharedMemCallable");
    }

    virtual ~SharedMemCallable()
    {}

    void
    operator()()
    {
        LOG_INFO("Entering");

        int shmid = shmget(key_, 2*sizeof(uint64_t), 0666);
        VERIFY(shmid >= 0);
        uint64_t* p  = reinterpret_cast<uint64_t*>(shmat(shmid, NULL, 0));
        VERIFY(p != (uint64_t*) -1);
        LOG_INFO(id_ << ": Value of p[0] " << p[0]);
        VERIFY(p[0] == 0);
        uint64_t b = p[1];
        LOG_INFO(id_ << ": Value of b " << b);
        VERIFY(b < max_id_);
        p[0] = 1;
        usleep(10000);
        p[1] = b + 1;
        p[0] = 0;
        LOG_INFO("Exiting");
    }

    DECLARE_LOGGER("SharedMemCallable");

    virtual const yt::GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

private:
    const uint64_t id_ ;
    const key_t key_;
    const uint64_t max_id_;
    const yt::GracePeriod grace_period_;
};

}

TEST_F(LockTest, test_mutual_exclusion)
{
    {
        be::BackendConnectionInterfacePtr c(cm_->getConnection());
        if (not c->hasExtendedApi())
        {
            SUCCEED() << "these tests require a backend that supports the extended api";
            return;
        }
    }

    using CallableT = LockService::WithGlobalLock<yt::ExceptionPolicy::DisableExceptions,
                                                  SharedMemCallable,
                                                  &SharedMemCallable::info>::type_;

    const uint64_t max_test = 4;

    const key_t ipc_id = 5678;
    int shmid = shmget(ipc_id, 2*sizeof(uint64_t), IPC_CREAT | 0666);
    VERIFY(shmid >= 0);
    uint64_t* p = reinterpret_cast<uint64_t*>(shmat(shmid, NULL, 0));
    VERIFY(p != (uint64_t*) -1);
    volatile uint64_t& p0 = p[0];
    p0 = 0;
    volatile uint64_t& p1 = p[1];
    p1 = 0;

    std::vector<boost::thread> threads;
    std::vector<std::unique_ptr<CallableT>> locked_callables;
    std::vector<std::unique_ptr<SharedMemCallable>> callables;

    const yt::GracePeriod grace_period(boost::posix_time::milliseconds(100));
    for(unsigned i = 0; i < max_test; ++i)
    {
        callables.emplace_back(std::make_unique<SharedMemCallable>(i,
                                                                   ipc_id,
                                                                   max_test,
                                                                   grace_period));

        be::GlobalLockStorePtr
            lock_store(new be::LockStore(cm_->newBackendInterface(testNamespace())));

        locked_callables.emplace_back(std::make_unique<CallableT>(boost::ref(*callables[i]),
                                                                  CallableT::retry_connection_times_default(),
                                                                  CallableT::connection_retry_timeout_default(),
                                                                  lock_store,
                                                                  be::UpdateInterval(boost::posix_time::seconds(1))));

        threads.emplace_back(boost::thread(boost::ref(*locked_callables[i])));
    }

    for(unsigned i = 0; i < max_test; ++i)
    {
        threads[i].join();
        std::exception_ptr no_exception;
        EXPECT_TRUE(locked_callables[i]->exit_exception == no_exception);
    }

    EXPECT_EQ(0U, p0);
    EXPECT_EQ(max_test, p1);
}
namespace
{
class SettingCallable
{
public:
    SettingCallable(uint64_t& test)
        :test_(test)
    {
        VERIFY(test == 0);
    }
    void
    operator()()
    {
        boost::this_thread::sleep(boost::posix_time::seconds(5));
    }

    uint64_t& test_;
};

struct LockAwayThrower
{
public:
    explicit LockAwayThrower(be::GlobalLockStorePtr lock_store)
        : lock_store_(lock_store)
    {}

    DECLARE_LOGGER("LockAwayThrower");

    void
    operator()()
    {
        while(true)
        {
            try
            {
                boost::this_thread::sleep(boost::posix_time::milliseconds(100));
                lock_store_->erase();
                LOG_INFO("Threw away the lock");
                return;
            }
            catch(boost::thread_interrupted& i)
            {
                LOG_INFO("Exiting the thread that throws away the lock");
                return;
            }
            catch(...)
            {
                LOG_INFO("Unknow exception exiting the thread that throws away the lock -- lock not there yet probably");
            }
        }
    }

    be::GlobalLockStorePtr lock_store_;
};

}

TEST_F(LockTest, test_throwing_away_the_lock)
{
    {
        be::BackendConnectionInterfacePtr c(cm_->getConnection());
        if (not c->hasExtendedApi())
        {
            SUCCEED() << "these tests require a backend that supports the extended api";
            return;
        }
    }

    be::GlobalLockStorePtr
        lock_store(new be::LockStore(cm_->newBackendInterface(testNamespace())));

    LockAwayThrower thrower(lock_store);
    const yt::GracePeriod grace_period(boost::posix_time::milliseconds(100));
    boost::thread t(thrower);
    TestCallBack callback;

    LockService lock_service(yt::GracePeriod(boost::posix_time::seconds(1)),
                             &trampoline<TestCallBack>,
                             &callback,
                             lock_store,
                             be::UpdateInterval(boost::posix_time::seconds(1)));

    ASSERT_TRUE(lock_service.lock());

    boost::this_thread::sleep(boost::posix_time::seconds(7));
    EXPECT_TRUE(callback.called_);
    t.interrupt();
    t.join();
}

namespace
{

class SleepingCallable
{
public:
    explicit SleepingCallable(const be::Namespace& ns)
        : ns_(ns)
    {}

    virtual ~SleepingCallable()
    {}

    void
    operator()()
    {
        boost::this_thread::sleep(boost::posix_time::seconds(3));
        LOG_INFO("Finished for namespace " << ns_);
    }
    DECLARE_LOGGER("SleepingCallable");

    std::string
    info()
    {
        return ("SleepingCallable " + ns_.str());
    }

    const be::Namespace ns_;

    virtual const yt::GracePeriod&
    grace_period() const
    {
        static const yt::GracePeriod grace_period_(boost::posix_time::milliseconds(100));
        return grace_period_;
    }

};

}

TEST_F(LockTest, test_parallel_locks)
{
    {
        be::BackendConnectionInterfacePtr c(cm_->getConnection());
        if (not c->hasExtendedApi())
        {
            SUCCEED() << "these tests require a backend that supports the extended api";
            return;
        }
    }

    using CallableT = LockService::WithGlobalLock<yt::ExceptionPolicy::DisableExceptions,
                                                  SleepingCallable,
                                                  &SleepingCallable::info>::type_;

    const int num_tests = 16;
    std::vector<std::unique_ptr<WithRandomNamespace>> namespaces;

    for(int i = 0; i < num_tests; ++i)
    {
        namespaces.push_back(make_random_namespace());
    }

    std::vector<boost::thread> threads;
    std::vector<std::unique_ptr<CallableT>> locked_callables;
    std::vector<std::unique_ptr<SleepingCallable>> callables;

    for(int i = 0; i < num_tests; ++i)
    {
        be::GlobalLockStorePtr
            lock_store(new be::LockStore(cm_->newBackendInterface(namespaces[i]->ns())));

        callables.emplace_back(std::make_unique<SleepingCallable>(namespaces[i]->ns()));

        locked_callables.emplace_back(std::make_unique<CallableT>(boost::ref(*callables[i]),
                                                                  CallableT::retry_connection_times_default(),
                                                                  CallableT::connection_retry_timeout_default(),
                                                                  lock_store,
                                                                  be::UpdateInterval(boost::posix_time::seconds(1))));

        threads.emplace_back(boost::thread(boost::ref(*locked_callables[i])));
    }
    for(int i = 0; i < num_tests; ++i)
    {
        threads[i].join();
        std::exception_ptr no_exception;
        EXPECT_TRUE(locked_callables[i]->exit_exception == no_exception);
    }
}

TEST_F(LockTest, test_serial_locks)
{
    {
        be::BackendConnectionInterfacePtr c(cm_->getConnection());
        if (not c->hasExtendedApi())
        {
            SUCCEED() << "these tests require a backend that supports the extended api";
            return;
        }
    }

    using CallableT = LockService::WithGlobalLock<yt::ExceptionPolicy::DisableExceptions,
                                                  SleepingCallable,
                                                  &SleepingCallable::info>::type_;

    const int num_tests = 4;
    const int serial_num_tests = 16;

    std::vector<std::unique_ptr<WithRandomNamespace> > namespaces;

    for(int i = 0; i < num_tests; ++i)
    {
        namespaces.push_back(make_random_namespace());
    }

    for(int k = 0; k < serial_num_tests; ++k)
    {
        std::vector<boost::thread> threads;
        std::vector<std::unique_ptr<CallableT>> locked_callables;
        std::vector<std::unique_ptr<SleepingCallable>> callables;

        for(int i = 0; i < num_tests; ++i)
        {
            be::GlobalLockStorePtr
                lock_store(new be::LockStore(cm_->newBackendInterface(namespaces[i]->ns())));

            callables.emplace_back(std::make_unique<SleepingCallable>(be::Namespace(namespaces[i]->ns())));

            locked_callables.emplace_back(std::make_unique<CallableT>(boost::ref(*callables[i]),
                                                                      yt::NumberOfRetries(10),
                                                                      CallableT::connection_retry_timeout_default(),
                                                                      lock_store,
                                                                      be::UpdateInterval(boost::posix_time::seconds(1))));
            threads.emplace_back(boost::thread(boost::ref(*locked_callables[i])));
        }

        for(int i = 0; i < num_tests; ++i)
        {
            threads[i].join();
            std::exception_ptr no_exception;
            EXPECT_TRUE(locked_callables[i]->exit_exception == no_exception);
        }
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
