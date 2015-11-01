// Copyright 2015 iNuron NV
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

#include "../Logging.h"
#include "../RWLock.h"
#include "../System.h"
#include "../TestBase.h"
#include "../wall_timer.h"

#include <boost/thread.hpp>

namespace youtilstest
{
// using namespace youtils;

template<typename L>
struct RWLockTraits;

template<>
struct RWLockTraits<boost::shared_mutex>
{
    typedef boost::shared_lock<boost::shared_mutex> ReadGuardType;
    typedef boost::unique_lock<boost::shared_mutex> WriteGuardType;
};


template<>
struct RWLockTraits<fungi::RWLock>
{
    typedef fungi::ScopedReadLock ReadGuardType;
    typedef fungi::ScopedWriteLock WriteGuardType;
};

class RWLockTest
    : public TestBase
{
protected:
    RWLockTest()
        : rwLock_("RWLockTest")
    {};

    DECLARE_LOGGER("RWLockTest");

    fungi::RWLock rwLock_;

    template<typename LockType,
             typename LockTraits = RWLockTraits<LockType>>
    void
    test_rlock_timings(LockType& rwlock)
    {
        const uint64_t count =
            youtils::System::get_env_with_default("LOCK_ITERATIONS",
                                                  10000000ULL);

        youtils::wall_timer w;

        for (unsigned i = 0; i < count; ++i)
        {
            typename LockTraits::ReadGuardType r(rwlock);
        }

        double t = w.elapsed();
        LOG_INFO(count << " iterations took " << t <<
            " -> " << (count / t) << " rlocks per second");
    }

    template<typename LockType,
             typename LockTraits = RWLockTraits<LockType>>
    void
    test_wlock_timings(LockType& rwlock)
    {
        const uint64_t count =
            youtils::System::get_env_with_default("LOCK_ITERATIONS",
                                                  10000000ULL);

        youtils::wall_timer w;

        for (unsigned i = 0; i < count; ++i)
        {
            typename LockTraits::WriteGuardType w(rwlock);
        }

        double t = w.elapsed();
        LOG_INFO(count << " iterations took " << t <<
            " -> " << (count / t) << " wlocks per second");
    }

    template<typename LockType,
             typename LockTraits = RWLockTraits<LockType>>
    void
    test_contention(LockType& rwlock)
    {
        unsigned num_readers = ::sysconf(_SC_NPROCESSORS_ONLN);
        num_readers = (num_readers > 1) ? (num_readers - 1) : 1;
        num_readers = std::min(num_readers, (unsigned)4);

        num_readers = youtils::System::get_env_with_default("LOCK_THREADS",
                                                            num_readers);
        std::vector<boost::thread> readers;
        readers.reserve(num_readers);

        const uint64_t max =
            youtils::System::get_env_with_default("LOCK_ITERATIONS",
                                                  1000000ULL);
        uint64_t count = 0;

        youtils::wall_timer w;

        for (unsigned i = 0; i < num_readers; ++i)
        {
            readers.emplace_back([&]{
                    while (true)
                    {
                        typename LockTraits::ReadGuardType
                            r(rwlock);
                        if (count >= max)
                        {
                            break;
                        }
                    }
                });
        }

        for (uint64_t i = 0; i < max; ++i)
        {
            typename LockTraits::WriteGuardType w(rwlock);
            ++count;
        }

        double t = w.elapsed();
        LOG_INFO("1 writer, " << num_readers << " readers: " <<
                 count << " iterations took " << t <<
                 " -> " << (count / t) << " locks per second");

        for (auto& r : readers)
        {
            r.join();
        }
    }
};

TEST_F(RWLockTest, basics)
{
    EXPECT_TRUE(rwLock_.tryReadLock());
    EXPECT_TRUE(rwLock_.tryReadLock());
    EXPECT_FALSE(rwLock_.tryWriteLock());
    rwLock_.unlock();
    EXPECT_FALSE(rwLock_.tryWriteLock());
    rwLock_.unlock();
    EXPECT_TRUE(rwLock_.tryWriteLock());
    EXPECT_FALSE(rwLock_.tryReadLock());
    EXPECT_FALSE(rwLock_.tryWriteLock());
    rwLock_.unlock();
    EXPECT_TRUE(rwLock_.tryReadLock());
    rwLock_.unlock();
}

TEST_F(RWLockTest, assertLocked)
{
    rwLock_.readLock();
    rwLock_.assertLocked();

    rwLock_.unlock();

    rwLock_.writeLock();
    rwLock_.assertLocked();

    rwLock_.unlock();
}

TEST_F(RWLockTest, fungi_rlock_timing)
{
    fungi::ScopedReadLock r(rwLock_);
    test_rlock_timings(rwLock_);
}

TEST_F(RWLockTest, boost_rlock_timing)
{
    boost::shared_mutex m;
    RWLockTraits<boost::shared_mutex>::ReadGuardType r(m);
    test_rlock_timings(m);
}

TEST_F(RWLockTest, fungi_wlock_timing)
{
    test_wlock_timings(rwLock_);
}

TEST_F(RWLockTest, boost_wlock_timing)
{
    boost::shared_mutex m;
    test_wlock_timings(m);
}

TEST_F(RWLockTest, fungi_contention)
{
    test_contention(rwLock_);
}

TEST_F(RWLockTest, boost_contention)
{
    boost::shared_mutex m;
    test_contention(m);
}

}

// Local Variables: **
// mode: c++ **
// End: **
