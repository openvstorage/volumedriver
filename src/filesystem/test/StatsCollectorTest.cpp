// Copyright 2016 iNuron NV
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

#include "../StatsCollector.h"

#include <chrono>
#include <thread>

#include <youtils/TestBase.h>

namespace volumedriverfstest
{

using namespace volumedriverfs;

class StatsCollectorTest
    : public youtilstest::TestBase
{};

TEST_F(StatsCollectorTest, simple)
{
    std::vector<std::string> dst;
    const std::string src("some message");

    std::atomic<uint64_t> pull_interval(1);

    std::vector<StatsCollector::PullFun> pull_funs =
        { [&src](StatsCollector& pusher)
          {
              pusher.push(src);
          }
        };

    StatsCollector::PushFun push_fun = [&dst](const std::string& msg)
        {
            dst.push_back(msg);
        };

    ASSERT_TRUE(dst.empty());

    StatsCollector pusher(pull_interval,
                       pull_funs,
                       push_fun);

    std::this_thread::sleep_for(std::chrono::seconds(pull_interval * 2));

    ASSERT_FALSE(dst.empty());
    EXPECT_EQ(src,
              dst[0]);
}

}
