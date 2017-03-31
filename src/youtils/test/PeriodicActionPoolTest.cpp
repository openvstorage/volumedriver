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
#include "../PeriodicActionPool.h"
#include "../Timer.h"

#include <gtest/gtest.h>

#include <boost/thread/reverse_lock.hpp>
#include <boost/thread/future.hpp>

namespace youtilstest
{

using namespace youtils;

class PeriodicActionPoolTest
    : public testing::Test
{
protected:
    DECLARE_LOGGER("PeriodicActionPoolTest");

    const size_t nthreads_ = 4;
};

TEST_F(PeriodicActionPoolTest, start_n_stop)
{
    PeriodicActionPool::create("TestPool",
                               nthreads_);
}

TEST_F(PeriodicActionPoolTest, no_threads)
{
    EXPECT_THROW(PeriodicActionPool::create("TestPool",
                                            0),
                 std::exception);
}

TEST_F(PeriodicActionPoolTest, basics)
{
    PeriodicActionPool::Ptr pool(PeriodicActionPool::create("TestPool",
                                                            nthreads_));

    const size_t iterations = 10;
    size_t count = 0;

    boost::promise<bool> promise;
    boost::unique_future<bool> future(promise.get_future());

    auto fun([&]() -> PeriodicActionContinue
             {
                 if (++count == iterations)
                 {
                     promise.set_value(true);
                 }

                 return PeriodicActionContinue::T;
             });

    std::atomic<uint64_t> period(10);

    PeriodicActionPool::Task t(pool->create_task("counter",
                                                 std::move(fun),
                                                 period,
                                                 false));

    EXPECT_TRUE(future.get());
}

TEST_F(PeriodicActionPoolTest, cancellation)
{
    PeriodicActionPool::Ptr pool(PeriodicActionPool::create("TestPool",
                                                            nthreads_));

    boost::promise<bool> promise;
    boost::unique_future<bool> future(promise.get_future());
    bool running = false;

    auto fun([&]() -> PeriodicActionContinue
             {
                 if (not running)
                 {
                     promise.set_value(true);
                     running = true;
                 }

                 return PeriodicActionContinue::T;
             });

    std::atomic<uint64_t> period(3600);

    PeriodicActionPool::Task t(pool->create_task("LongPeriod",
                                                 std::move(fun),
                                                 period,
                                                 true));

    EXPECT_TRUE(future.get());
}

TEST_F(PeriodicActionPoolTest, lotsa)
{
    PeriodicActionPool::Ptr pool(PeriodicActionPool::create("TestPool",
                                                            nthreads_));

    boost::promise<bool> promise;
    boost::unique_future<bool> future(promise.get_future());

    const uint64_t limit = 65536;
    const size_t ntasks = 1024;

    std::atomic<uint64_t> val(0);

    auto fun([&]() -> PeriodicActionContinue
             {
                 if (val.fetch_add(1) == limit)
                 {
                     promise.set_value(true);
                 }

                 return PeriodicActionContinue::T;
             });

    std::atomic<uint64_t> period(5);
    std::vector<PeriodicActionPool::Task> vec;
    vec.reserve(ntasks);

    for (size_t i = 0; i < ntasks; ++i)
    {
        vec.emplace_back(pool->create_task("CounterAgain",
                                           fun,
                                           period,
                                           false));
    }

    EXPECT_TRUE(future.get());
}

TEST_F(PeriodicActionPoolTest, ramp_up)
{
    PeriodicActionPool::Ptr pool(PeriodicActionPool::create("TestPool",
                                                            nthreads_));

    boost::promise<bool> promise;
    boost::unique_future<bool> future(promise.get_future());
    bool running = false;

    auto fun([&]() -> PeriodicActionContinue
             {
                 if (not running)
                 {
                     promise.set_value(true);
                     running = true;
                 }

                 return PeriodicActionContinue::T;
             });

    const size_t ramp_up_ms = 200;
    const std::atomic<uint64_t> period(3600);

    SteadyTimer timer;

    PeriodicActionPool::Task
        task(pool->create_task("RampedUp",
                               fun,
                               period,
                               true,
                               std::chrono::milliseconds(ramp_up_ms)));

    EXPECT_TRUE(future.get());
    EXPECT_LT(boost::chrono::milliseconds(ramp_up_ms),
              timer.elapsed());
}

TEST_F(PeriodicActionPoolTest, self_termination)
{
    PeriodicActionPool::Ptr pool(PeriodicActionPool::create("TestPool",
                                                            nthreads_));

    size_t count = 0;

    auto fun([&]() -> PeriodicActionContinue
             {
                 ++count;
                 return PeriodicActionContinue::F;
             });

    const std::atomic<uint64_t> period(1);

    SteadyTimer timer;

    PeriodicActionPool::Task
        task(pool->create_task("OneShot",
                               fun,
                               period,
                               false));

    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    EXPECT_EQ(1,
              count);
}

TEST_F(PeriodicActionPoolTest, fast_cancellation)
{
    PeriodicActionPool::Ptr pool(PeriodicActionPool::create("TestPool",
                                                            1));

    boost::mutex mutex;
    boost::condition_variable cond;

    enum class State
    {
        Start,
        Block,
        Unblock,
        Stop
    };

    State state = State::Start;

    auto blocker_fun([&]() -> PeriodicActionContinue
                     {
                         boost::unique_lock<decltype(mutex)> u(mutex);
                         state = State::Block;
                         cond.notify_one();
                         cond.wait(u,
                                   [&]() -> bool
                                   {
                                       return state == State::Unblock;
                                   });

                         state = State::Stop;
                         cond.notify_one();
                         return PeriodicActionContinue::F;
                     });

    boost::unique_lock<decltype(mutex)> u(mutex);

    const std::atomic<uint64_t> period(1);

    PeriodicActionPool::Task
        blocker(pool->create_task("Blocker",
                                  std::move(blocker_fun),
                                  period,
                                  false));

    cond.wait(u,
              [&]() -> bool
              {
                  return state == State::Block;
              });

    {
        boost::reverse_lock<decltype(u)> r(u);
        bool ran = false;

        auto blocked_fun([&]() -> PeriodicActionContinue
                         {
                             ran = true;
                             return PeriodicActionContinue::T;
                         });

        PeriodicActionPool::Task
            blocked(pool->create_task("Blocked",
                                      std::move(blocked_fun),
                                      period,
                                      false,
                                      std::chrono::milliseconds(0)));

        EXPECT_FALSE(ran);
    }

    state = State::Unblock;
    cond.notify_one();

    cond.wait(u,
              [&]() -> bool
              {
                  return state == State::Stop;
              });
}

}
