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

#include "../ChronoUtils.h"

#include <gtest/gtest.h>

namespace youtilstest
{

using namespace youtils;
namespace bc = boost::chrono;

TEST(ChronoUtilsTest, boost_steady_clock_abs_timespec)
{
    using Clock = bc::steady_clock;
    Clock::time_point now(Clock::now());
    const Clock::duration duration(bc::seconds(10));

    timespec ts;
    ASSERT_EQ(0, clock_gettime(CLOCK_REALTIME, &ts));

    EXPECT_EQ(ts.tv_sec,
              ChronoUtils::abs_timespec(now).tv_sec);

    timespec future;
    future.tv_sec = ts.tv_sec + 10;
    future.tv_nsec = ts.tv_nsec;

    EXPECT_EQ(future.tv_sec,
              ChronoUtils::abs_timespec(duration).tv_sec);

    timespec past;
    past.tv_sec = ts.tv_sec - 10;
    past.tv_nsec = ts.tv_nsec;

    EXPECT_EQ(past.tv_sec,
              ChronoUtils::abs_timespec(now - duration).tv_sec);
}

}
