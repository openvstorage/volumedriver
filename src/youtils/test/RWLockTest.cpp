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

#include "../Logging.h"
#include "../RWLock.h"
#include "../System.h"
#include "../Timer.h"

#include <gtest/gtest.h>

#include "../wall_timer.h"

#include <boost/chrono.hpp>
#include <boost/thread.hpp>

namespace youtilstest
{

using namespace youtils;
namespace bc = boost::chrono;

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
    : public testing::Test
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
            System::get_env_with_default("LOCK_ITERATIONS",
                                                  10000000ULL);

        wall_timer w;

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
            System::get_env_with_default("LOCK_ITERATIONS",
                                                  10000000ULL);

        wall_timer w;

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

        num_readers = System::get_env_with_default("LOCK_THREADS",
                                                            num_readers);
        std::vector<boost::thread> readers;
        readers.reserve(num_readers);

        const uint64_t max =
            System::get_env_with_default("LOCK_ITERATIONS",
                                                  1000000ULL);
        uint64_t count = 0;

        wall_timer w;

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

    template<typename LockType,
             typename LockTraits = RWLockTraits<LockType>>
    void
    test_read_contention(LockType& rwlock)
    {
        unsigned num_readers = ::sysconf(_SC_NPROCESSORS_ONLN);
        num_readers = std::min(num_readers, (unsigned)4);

        num_readers = System::get_env_with_default("LOCK_THREADS",
                                                            num_readers);
        std::vector<boost::thread> readers;
        readers.reserve(num_readers);

        const uint64_t max =
            System::get_env_with_default("LOCK_ITERATIONS",
                                         1000000ULL);
        std::atomic<uint64_t> count(0);

        wall_timer w;

        for (unsigned i = 0; i < num_readers; ++i)
        {
            readers.emplace_back([&]{
                    while (true)
                    {
                        typename LockTraits::ReadGuardType
                            r(rwlock);
                        if (count++ >= max)
                        {
                            break;
                        }
                    }
                });
        }

        for (auto& r : readers)
        {
            r.join();
        }

        double t = w.elapsed();
        LOG_INFO(num_readers  << " readers: " <<
                 count << " iterations took " << t <<
                 " -> " << (count / t) << " locks per second");
    }

    template<typename Fst, typename Snd>
    void
    test_timeout()
    {
        using Clock = bc::steady_clock;
        const Clock::duration timeout(bc::milliseconds(500));

        {
            Fst f(rwLock_);
            ASSERT_TRUE(f);

            std::async(std::launch::async,
                       [&]
                       {
                           const Clock::time_point deadline = Clock::now() + timeout;

                           {
                               SteadyTimer t;
                               Snd s(rwLock_, deadline);
                               EXPECT_FALSE(s);

                               EXPECT_LE(timeout,
                                         t.elapsed());
                               EXPECT_GT(timeout * 2,
                                         t.elapsed());
                           }
                           {
                               SteadyTimer t;
                               Snd s(rwLock_, timeout);
                               EXPECT_FALSE(s);

                               EXPECT_LE(timeout,
                                         t.elapsed());
                               EXPECT_GT(timeout * 2,
                                         t.elapsed());
                           }
                       }).wait();
        }

        {
            const Clock::time_point deadline = Clock::now() + timeout;
            SteadyTimer t;
            Snd s(rwLock_,
                  deadline);

            EXPECT_TRUE(s);
            EXPECT_GT(timeout,
                      t.elapsed());
        }

        {
            SteadyTimer t;
            Snd s(rwLock_,
                  timeout);

            EXPECT_TRUE(s);
            EXPECT_GT(timeout,
                      t.elapsed());
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

TEST_F(RWLockTest, fungi_read_contention)
{
    test_read_contention(rwLock_);
}

TEST_F(RWLockTest, boost_read_contention)
{
    boost::shared_mutex m;
    test_read_contention(m);
}

TEST_F(RWLockTest, wtimeout)
{
    test_timeout<boost::shared_lock<decltype(rwLock_)>,
                 boost::unique_lock<decltype(rwLock_)>>();
}

TEST_F(RWLockTest, rtimeout)
{
    test_timeout<boost::unique_lock<decltype(rwLock_)>,
                 boost::shared_lock<decltype(rwLock_)>>();
}

}

// Local Variables: **
// mode: c++ **
// End: **
