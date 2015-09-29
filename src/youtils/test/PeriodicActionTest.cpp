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

#include "../TestBase.h"
#include "../PeriodicAction.h"
#include "../Assert.h"
#include "../SourceOfUncertainty.h"

#include <future>
#include <thread>
#include <iostream>

#include <math.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/ptr_container/ptr_vector.hpp>


namespace youtilstest
{

namespace bpt = boost::posix_time;
using namespace youtils;

using namespace std::string_literals;

class PeriodicActionTest
    : public TestBase
{
protected:
    DECLARE_LOGGER("PeriodicActionTest");
};

namespace
{

uint64_t
duration(unsigned ix)
{
    return ix;
}

}

TEST_F(PeriodicActionTest, stop)
{
    auto act(std::make_unique<PeriodicAction>("nothing interesting, really",
                                              []
                                              {
                                                  LOG_INFO("going to sleep");
                                                  boost::this_thread::sleep_for(boost::chrono::seconds(60));
                                              },
                                              std::numeric_limits<uint64_t>::max(),
                                              true,
                                              boost::chrono::milliseconds(30000)));

    boost::this_thread::sleep_for(boost::chrono::seconds(2));
    act.reset();
}

TEST_F(PeriodicActionTest, Ztrezzy)
{
    const uint64_t num_runs = 128;
        // 1024;
    youtils::SourceOfUncertainty sou;

    std::atomic<uint64_t> duration;

    for(uint64_t i = 0; i < num_runs; ++i)
    {
        duration.store(sou(31));

        PeriodicAction action(std::string("boring action pt. ") +
                              boost::lexical_cast<std::string>(i),
                              []
                              {
                              },
                              duration,
                              false);
        uint32_t reset = sou(31);

        for(unsigned j = 0; j < reset; ++j)
        {
            usleep(sou(200));
            duration.store(sou(31));
        }
    }
}

TEST_F(PeriodicActionTest, General)
{
    const unsigned n = 5;
    std::vector<int> x(n, 0);
    boost::ptr_vector<std::atomic<uint64_t> > periods(n);
    periods.reserve(n);

    std::vector<std::unique_ptr<PeriodicAction>> acts;
    acts.reserve(n);

    bpt::ptime start = bpt::second_clock::universal_time();
    for (unsigned i = 0; i < n; i++)
    {
        periods.push_back(new std::atomic<uint64_t>(i + 1));
        acts.emplace_back(new PeriodicAction(std::string("incrementer ") +
                                             boost::lexical_cast<std::string>(i),
                                             [&x, i]
                                             {
                                                 ++x[i];
                                                 std::this_thread::sleep_for(std::chrono::seconds(duration(i)));
                                             },
                                             periods[i]));
    }

    const int slp = 7;
    sleep(slp);

    for (unsigned i = 0; i < n; i++)
    {
        const bpt::time_duration d = bpt::microsec_clock::universal_time() - start;
        acts[i].reset();
        const int expected = floor(d.total_milliseconds() / 1000.0) /
            std::max(periods[i].load(), duration(i));
        EXPECT_NEAR(x[i], expected, 1.1);
    }
}

TEST_F(PeriodicActionTest, seppuku)
{
    unsigned count = 0;

    std::atomic<uint64_t> period(1);

    PeriodicAction::AbortableAction a([&]() -> auto
                                      {
                                          ++count;
                                          return PeriodicActionContinue::F;
                                      });

    PeriodicAction act("run once"s,
                       std::move(a),
                       period);

    boost::this_thread::sleep_for(boost::chrono::seconds(5));
    EXPECT_EQ(1, count);
}

// This is not expected to succeed at the moment, as locking a boost mutex
// is not an interruption point. IOW: it will deadlock.
TEST_F(PeriodicActionTest, DISABLED_stop_while_trying_to_acquire_lock)
{
    boost::mutex lock;
    boost::lock_guard<decltype(lock)> g(lock);
    std::atomic<uint64_t> period(1);
    std::promise<bool> running;

    auto act(std::make_unique<PeriodicAction>("locker"s,
                                              [&lock,
                                               &running]
                                              {
                                                  running.set_value(true);
                                                  boost::lock_guard<decltype(lock)>
                                                      g(lock);
                                                  FAIL() <<
                                                      "This should never be reached. Ever.";
                                              },
                                              period));

    ASSERT_TRUE(running.get_future().get());

    act.reset();
}

}

// Local Variables: **
// mode: c++ **
// End: **
