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

#include <map>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <youtils/TestBase.h>
#include <youtils/Weed.h>
#include <youtils/WithGlobalLock.h>

#include "../BackendConnectionManager.h"
#include "../BackendInterface.h"
#include "../HeartBeatLockService.h"
#include "../Lock.h"
#include "../LockCommunicator.h"

namespace backendtest
{
namespace be = backend;
namespace yt = youtils;

using namespace std::literals::string_literals;
using yt::WithGlobalLock;

namespace
{

BOOLEAN_ENUM(MustBeEmpty);

struct Locks
{
    boost::mutex lock;
    std::map<yt::UUID, std::string> map;

    decltype(map)::iterator
    find(const yt::UUID& uuid,
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
    : public be::GlobalLockStore
{
public:
    LockStore(Locks& locks,
              const yt::UUID& uuid)
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

    std::tuple<be::Lock, be::LockTag>
    read() override final
    {
        boost::lock_guard<decltype(Locks::lock)> g(locks_.lock);

        auto it = locks_.find(uuid_,
                              MustBeEmpty::F);

        EXPECT_TRUE(locks_.map.end() != it);
        EXPECT_FALSE(it->second.empty());

        return
            std::make_tuple(be::Lock(it->second),
                            boost::lexical_cast<be::LockTag>(yt::Weed(reinterpret_cast<const byte*>(it->second.data()),
                                                                      it->second.size())));
    }

    be::LockTag
    write(const be::Lock& lock,
          const boost::optional<be::LockTag>& tag) override final
    {
        boost::lock_guard<decltype(Locks::lock)> g(locks_.lock);

        auto it = locks_.find(uuid_,
                              tag ? MustBeEmpty::F : MustBeEmpty::T);
        EXPECT_TRUE(locks_.map.end() != it);

        if (tag)
        {
            EXPECT_FALSE(it->second.empty());
            const auto current_tag(boost::lexical_cast<be::LockTag>(yt::Weed(reinterpret_cast<const byte*>(it->second.data()),
                                                                             it->second.size())));
            if (*tag != current_tag)
            {
                throw std::runtime_error("lock has changed for "s + name());
            }
        }

        std::string s;
        lock.save(s);
        locks_.map[uuid_] = s;

        return boost::lexical_cast<be::LockTag>(yt::Weed(reinterpret_cast<const byte*>(s.data()),
                                                         s.size()));
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
    DECLARE_LOGGER("BackendLockStore");

    Locks& locks_;
    yt::UUID uuid_;
    std::string name_;
};

DECLARE_LOGGER("LockTest");

}

class LockTest
    : public youtilstest::TestBase
{
public:
    using LockService = be::HeartBeatLockService;
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

TEST_F(LockTest, test_no_lock_if_namespace_doesnt_exist)
{
    Locks locks;
    TestCallBack callback;

    be::GlobalLockStorePtr
        lock_store(new LockStore(locks,
                                 yt::UUID()));

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
    const yt::GracePeriod grace_period(boost::posix_time::milliseconds(100));
    TestCallBack callback;

    yt::UUID uuid;
    Locks locks;
    locks.map = { { uuid, std::string() } };

    be::GlobalLockStorePtr
        lock_store(new LockStore(locks,
                                 uuid));

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
    using CallableT = LockService::WithGlobalLock<yt::ExceptionPolicy::DisableExceptions,
                                                  SharedMemCallable,
                                                  &SharedMemCallable::info>::type_;

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

    yt::UUID uuid;
    Locks locks;
    locks.map = { { uuid, std::string() } };

    const yt::GracePeriod grace_period(boost::posix_time::milliseconds(100));
    for(unsigned i = 0; i < max_test; ++i)
    {
        callables.emplace_back(std::make_unique<SharedMemCallable>(i,
                                                                   ipc_id,
                                                                   max_test,
                                                                   grace_period));

        be::GlobalLockStorePtr
            lock_store(new LockStore(locks,
                                     uuid));

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
    yt::UUID uuid;
    Locks locks;
    locks.map = {{ uuid, std::string() }};

    be::GlobalLockStorePtr
        lock_store(new LockStore(locks,
                                 uuid));

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

struct SleepingCallable
{
    DECLARE_LOGGER("SleepingCallable");

    const yt::UUID uuid_;

    explicit SleepingCallable(const yt::UUID& uuid)
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
    using CallableT = LockService::WithGlobalLock<yt::ExceptionPolicy::DisableExceptions,
                                                  SleepingCallable,
                                                  &SleepingCallable::info>::type_;

    const int num_tests = 16;
    std::vector<yt::UUID> uuids(num_tests);

    Locks locks;

    for (const auto& u : uuids)
    {
        locks.map.emplace(std::make_pair(u,
                                         std::string()));
    }

    std::vector<boost::thread> threads;
    std::vector<std::unique_ptr<CallableT>> locked_callables;
    std::vector<std::unique_ptr<SleepingCallable>> callables;

    for(int i = 0; i < num_tests; ++i)
    {
        be::GlobalLockStorePtr
            lock_store(new LockStore(locks,
                                     uuids[i]));

        callables.emplace_back(std::make_unique<SleepingCallable>(uuids[i]));
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
    using CallableT = LockService::WithGlobalLock<yt::ExceptionPolicy::DisableExceptions,
                                                  SleepingCallable,
                                                  &SleepingCallable::info>::type_;

    const int num_tests = 4;
    const int serial_num_tests = 16;

    std::vector<yt::UUID> uuids(num_tests);

    Locks locks;

    for (const auto& u : uuids)
    {
        locks.map.emplace(std::make_pair(u,
                                         std::string()));
    }

    for(int k = 0; k < serial_num_tests; ++k)
    {
        std::vector<boost::thread> threads;
        std::vector<std::unique_ptr<CallableT>> locked_callables;
        std::vector<std::unique_ptr<SleepingCallable>> callables;

        for(int i = 0; i < num_tests; ++i)
        {
            be::GlobalLockStorePtr
                lock_store(new LockStore(locks,
                                         uuids[i]));

            callables.emplace_back(std::make_unique<SleepingCallable>(uuids[i]));

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
