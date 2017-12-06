// Copyright (C) 2017 iNuron NV
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

#include <gtest/gtest.h>

#include "../Timer.h"

#include <boost/chrono.hpp>
#include <boost/thread.hpp>

namespace youtilstest
{

using namespace youtils;
namespace bc = boost::chrono;

TEST(SteadyTimerTest, basic)
{
    SteadyTimer t;
    const bc::milliseconds timeout(1);
    boost::this_thread::sleep_for(timeout);
    EXPECT_LE(timeout,
              t.elapsed());
}

TEST(SteadyTimerTest, copy)
{
    SteadyTimer t1;
    const bc::milliseconds timeout(1);
    boost::this_thread::sleep_for(timeout);
    SteadyTimer t2(t1);
    EXPECT_LE(timeout,
              t1.elapsed());
}

}
