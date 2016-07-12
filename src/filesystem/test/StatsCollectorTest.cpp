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

#include "../StatsCollector.h"

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

namespace volumedriverfstest
{

using namespace volumedriverfs;

class StatsCollectorTest
    : public testing::Test
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
