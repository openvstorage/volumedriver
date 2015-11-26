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

#include "../Logging.h"
#include "../PeriodicActionPool.h"
#include "../TestBase.h"
#include "../Timer.h"

#include <boost/thread/future.hpp>

namespace youtilstest
{

using namespace youtils;

class PeriodicActionPoolTest
    : public TestBase
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

    std::unique_ptr<PeriodicActionPool::Task> t(pool->create_task("counter",
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

    std::unique_ptr<PeriodicActionPool::Task> t(pool->create_task("LongPeriod",
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
    std::vector<std::unique_ptr<PeriodicActionPool::Task>> vec;
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

    std::unique_ptr<PeriodicActionPool::Task>
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

    std::unique_ptr<PeriodicActionPool::Task>
        task(pool->create_task("OneShot",
                               fun,
                               period,
                               false));

    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    EXPECT_EQ(1,
              count);
}

}
