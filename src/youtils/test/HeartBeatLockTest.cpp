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

#include "../GlobalLockStore.h"
#include "../HeartBeatLockService.h"
#include "../HeartBeatLockService.h"
#include "../HeartBeatLockCommunicator.h"
#include "../ObjectMd5.h"
#include "../Weed.h"
#include "../WithGlobalLock.h"

#include <map>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <boost/make_shared.hpp>

#include <gtest/gtest.h>

namespace youtilstest
{

using namespace std::literals::string_literals;
using namespace youtils;

namespace
{

VD_BOOLEAN_ENUM(MustBeEmpty);

struct Locks
{
    boost::mutex lock;
    std::map<UUID, std::string> map;

    decltype(map)::iterator
    find(const UUID& uuid,
         MustBeEmpty must_be_empty)
    {
        auto it = map.find(uuid);
        if (it == map.end())
        {
            throw std::runtime_error(uuid.str() + " not found"s);
        }

        if (must_be_empty == MustBeEmpty::T and
            not it->second.empty())
        {
            throw std::runtime_error(uuid.str() + " does not have empty value");
        }

        return it;
    }
};

class LockStore
    : public GlobalLockStore
{
public:
    LockStore(Locks& locks,
              const UUID& uuid)
        : locks_(locks)
        , uuid_(uuid)
        , name_(uuid_.str())
    {}

    virtual ~LockStore() = default;

    LockStore(const LockStore&) = delete;

    LockStore&
    operator=(const LockStore&) = delete;

    bool
    exists() override final
    {
        boost::lock_guard<decltype(Locks::lock)> g(locks_.lock);

        auto it = locks_.find(uuid_,
                              MustBeEmpty::F);
        return not it->second.empty();
    }

    std::tuple<HeartBeatLock, std::unique_ptr<UniqueObjectTag>>
    read() override final
    {
        boost::lock_guard<decltype(Locks::lock)> g(locks_.lock);

        auto it = locks_.find(uuid_,
                              MustBeEmpty::F);

        EXPECT_TRUE(locks_.map.end() != it);
        EXPECT_FALSE(it->second.empty());

        const Weed weed(reinterpret_cast<const byte*>(it->second.data()),
                        it->second.size());
        return std::make_tuple(HeartBeatLock(it->second),
                               std::make_unique<ObjectMd5>(weed));
    }

    std::unique_ptr<UniqueObjectTag>
    write(const HeartBeatLock& lock,
          const UniqueObjectTag* tag) override final
    {
        boost::lock_guard<decltype(Locks::lock)> g(locks_.lock);

        auto it = locks_.find(uuid_,
                              tag ? MustBeEmpty::F : MustBeEmpty::T);
        EXPECT_TRUE(locks_.map.end() != it);

        if (tag)
        {
            EXPECT_FALSE(it->second.empty());

            const Weed weed(reinterpret_cast<const byte*>(it->second.data()),
                        it->second.size());

            const auto current_tag(std::make_unique<ObjectMd5>(weed));
            if (*tag != *current_tag)
            {
                throw std::runtime_error("lock has changed for "s + name());
            }
        }

        std::string s;
        lock.save(s);
        locks_.map[uuid_] = s;

        const Weed weed(reinterpret_cast<const byte*>(s.data()),
                        s.size());
        return std::make_unique<ObjectMd5>(weed);
    }

    const std::string&
    name() const override final
    {
        return name_;
    }

    void
    erase() override final
    {
        boost::lock_guard<decltype(Locks::lock)> g(locks_.lock);
        locks_.map.erase(uuid_);
    }

private:
    DECLARE_LOGGER("TestLockStore");

    Locks& locks_;
    UUID uuid_;
    std::string name_;
};

}

class HeartBeatLockTest
    : public testing::Test
{
protected:
    using LockService = HeartBeatLockService;

    HeartBeatLockTest()
        : testing::Test()
    {
        locks_.map = { { uuid_, std::string() } };
        lock_store_ = boost::make_shared<LockStore>(locks_,
                                                    uuid_);
    }

    UUID uuid_;
    Locks locks_;
    GlobalLockStorePtr lock_store_;
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

TEST_F(HeartBeatLockTest, test_no_lock_if_namespace_doesnt_exist)
{
    TestCallBack callback;

    locks_.map.clear();

    LockService lock_service(GracePeriod(boost::posix_time::seconds(1)),
                             &trampoline<TestCallBack>,
                             &callback,
                             lock_store_,
                             UpdateInterval(boost::posix_time::seconds(1)));

    ASSERT_FALSE(lock_service.lock());
    EXPECT_NO_THROW(lock_service.unlock());
    sleep(1);
    EXPECT_FALSE(callback.called_);
}

TEST_F(HeartBeatLockTest, test_lock_if_namespace_exists)
{
    const GracePeriod grace_period(boost::posix_time::milliseconds(100));
    TestCallBack callback;

    LockService lock_service(GracePeriod(boost::posix_time::seconds(1)),
                             &trampoline<TestCallBack>,
                             &callback,
                             lock_store_,
                             UpdateInterval(boost::posix_time::seconds(1)));

    ASSERT_TRUE(lock_service.lock());
    EXPECT_NO_THROW(lock_service.unlock());
    sleep(1);
    EXPECT_TRUE(callback.called_);
}

namespace
{

class SharedMemCallable
    : public GlobalLockedCallable
{
public:
    SharedMemCallable(const uint64_t id,
                      const key_t key,
                      const uint64_t max_id,
                      const GracePeriod grace_period)
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
        ASSERT_LE(0, shmid);
        uint64_t* p  = reinterpret_cast<uint64_t*>(shmat(shmid, NULL, 0));
        ASSERT_NE(reinterpret_cast<uint64_t*>(-1),
                  p);
        LOG_INFO(id_ << ": Value of p[0] " << p[0]);
        ASSERT_EQ(0,
                  p[0]);
        uint64_t b = p[1];
        LOG_INFO(id_ << ": Value of b " << b);
        ASSERT_GT(max_id_,
                  b);
        p[0] = 1;
        usleep(10000);
        p[1] = b + 1;
        p[0] = 0;
        LOG_INFO("Exiting");
    }

    DECLARE_LOGGER("SharedMemCallable");

    virtual const GracePeriod&
    grace_period() const
    {
        return grace_period_;
    }

private:
    const uint64_t id_ ;
    const key_t key_;
    const uint64_t max_id_;
    const GracePeriod grace_period_;
};

}

TEST_F(HeartBeatLockTest, test_mutual_exclusion)
{
    using CallableT = LockService::WithGlobalLock<ExceptionPolicy::DisableExceptions,
                                                  SharedMemCallable,
                                                  &SharedMemCallable::info>;

    const uint64_t max_test = 4;

    const key_t ipc_id = 5678;
    int shmid = shmget(ipc_id, 2*sizeof(uint64_t), IPC_CREAT | 0666);
    ASSERT_LE(0,
              shmid);
    uint64_t* p = reinterpret_cast<uint64_t*>(shmat(shmid, NULL, 0));
    ASSERT_NE(reinterpret_cast<uint64_t*>(-1),
              p);
    volatile uint64_t& p0 = p[0];
    p0 = 0;
    volatile uint64_t& p1 = p[1];
    p1 = 0;

    std::vector<boost::thread> threads;
    std::vector<std::unique_ptr<CallableT>> locked_callables;
    std::vector<std::unique_ptr<SharedMemCallable>> callables;

    const GracePeriod grace_period(boost::posix_time::milliseconds(100));
    for(unsigned i = 0; i < max_test; ++i)
    {
        callables.emplace_back(std::make_unique<SharedMemCallable>(i,
                                                                   ipc_id,
                                                                   max_test,
                                                                   grace_period));

        GlobalLockStorePtr
            lock_store(boost::make_shared<LockStore>(locks_,
                                                     uuid_));

        locked_callables.emplace_back(std::make_unique<CallableT>(boost::ref(*callables[i]),
                                                                  CallableT::retry_connection_times_default(),
                                                                  CallableT::connection_retry_timeout_default(),
                                                                  lock_store,
                                                                  UpdateInterval(boost::posix_time::seconds(1))));

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
        EXPECT_EQ(0,
                  test);
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
    explicit LockAwayThrower(GlobalLockStorePtr lock_store)
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

    GlobalLockStorePtr lock_store_;
};

}

TEST_F(HeartBeatLockTest, test_throwing_away_the_lock)
{
    LockAwayThrower thrower(lock_store_);

    const GracePeriod grace_period(boost::posix_time::milliseconds(100));
    boost::thread t(thrower);
    TestCallBack callback;

    LockService lock_service(GracePeriod(boost::posix_time::seconds(1)),
                             &trampoline<TestCallBack>,
                             &callback,
                             lock_store_,
                             UpdateInterval(boost::posix_time::seconds(1)));

    ASSERT_TRUE(lock_service.lock());

    boost::this_thread::sleep(boost::posix_time::seconds(7));
    EXPECT_TRUE(callback.called_);
    t.interrupt();
    t.join();
}

namespace
{

struct SleepingCallable
{
    DECLARE_LOGGER("SleepingCallable");

    const UUID uuid_;

    explicit SleepingCallable(const UUID& uuid)
        : uuid_(uuid)
    {}

    virtual ~SleepingCallable() = default;

    void
    operator()()
    {
        boost::this_thread::sleep(boost::posix_time::seconds(3));
        LOG_INFO("Finished for " << uuid_);
    }

    std::string
    info()
    {
        return ("SleepingCallable "s + uuid_.str());
    }


    virtual const GracePeriod&
    grace_period() const
    {
        static const GracePeriod grace_period_(boost::posix_time::milliseconds(100));
        return grace_period_;
    }
};

}

TEST_F(HeartBeatLockTest, test_parallel_locks)
{
    using CallableT = LockService::WithGlobalLock<ExceptionPolicy::DisableExceptions,
                                                  SleepingCallable,
                                                  &SleepingCallable::info>;

    const int num_tests = 16;
    std::vector<UUID> uuids(num_tests);

    for (const auto& u : uuids)
    {
        locks_.map.emplace(std::make_pair(u,
                                          std::string()));
    }

    std::vector<boost::thread> threads;
    std::vector<std::unique_ptr<CallableT>> locked_callables;
    std::vector<std::unique_ptr<SleepingCallable>> callables;

    for(int i = 0; i < num_tests; ++i)
    {
        GlobalLockStorePtr
            lock_store(new LockStore(locks_,
                                     uuids[i]));

        callables.emplace_back(std::make_unique<SleepingCallable>(uuids[i]));
        locked_callables.emplace_back(std::make_unique<CallableT>(boost::ref(*callables[i]),
                                                                  CallableT::retry_connection_times_default(),
                                                                  CallableT::connection_retry_timeout_default(),
                                                                  lock_store,
                                                                  UpdateInterval(boost::posix_time::seconds(1))));

        threads.emplace_back(boost::thread(boost::ref(*locked_callables[i])));
    }
    for(int i = 0; i < num_tests; ++i)
    {
        threads[i].join();
        std::exception_ptr no_exception;
        EXPECT_TRUE(locked_callables[i]->exit_exception == no_exception);
    }
}

TEST_F(HeartBeatLockTest, test_serial_locks)
{
    using CallableT = LockService::WithGlobalLock<ExceptionPolicy::DisableExceptions,
                                                  SleepingCallable,
                                                  &SleepingCallable::info>;

    const int num_tests = 4;
    const int serial_num_tests = 16;

    std::vector<UUID> uuids(num_tests);

    for (const auto& u : uuids)
    {
        locks_.map.emplace(std::make_pair(u,
                                          std::string()));
    }

    for(int k = 0; k < serial_num_tests; ++k)
    {
        std::vector<boost::thread> threads;
        std::vector<std::unique_ptr<CallableT>> locked_callables;
        std::vector<std::unique_ptr<SleepingCallable>> callables;

        for(int i = 0; i < num_tests; ++i)
        {
            GlobalLockStorePtr
                lock_store(new LockStore(locks_,
                                         uuids[i]));

            callables.emplace_back(std::make_unique<SleepingCallable>(uuids[i]));

            locked_callables.emplace_back(std::make_unique<CallableT>(boost::ref(*callables[i]),
                                                                      NumberOfRetries(10),
                                                                      CallableT::connection_retry_timeout_default(),
                                                                      lock_store,
                                                                      UpdateInterval(boost::posix_time::seconds(1))));
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
