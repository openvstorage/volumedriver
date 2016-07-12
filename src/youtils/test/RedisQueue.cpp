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

#include "../RedisQueue.h"
#include <gtest/gtest.h>

#include <boost/lexical_cast.hpp>

namespace youtilstest
{
using namespace youtils;
using namespace std::literals::string_literals;

class RedisQueueTest
    : public testing::Test
{};

TEST_F(RedisQueueTest, simple_push_pop)
{
    const std::string empty;
    const std::string h("localhost");
    const uint16_t p = RedisUrl::default_port;
    const std::string k("/rqueue");
    const RedisUrl url(boost::lexical_cast<RedisUrl>("redis://"s +
                                                     h + ":"s +
                                                     boost::lexical_cast<std::string>(p) +
                                                     k));
    RedisQueue rq(url, 10);

    EXPECT_NO_THROW(rq.push(h));

    EXPECT_EQ(h,
              rq.pop<std::string>());
    EXPECT_EQ(empty,
              rq.pop<std::string>());
}

TEST_F(RedisQueueTest, fifo_order)
{
    const std::string empty;
    const std::string h("localhost");
    const uint16_t p = RedisUrl::default_port;
    const std::string k("/rqueue");
    const RedisUrl url(boost::lexical_cast<RedisUrl>("redis://"s +
                                                     h + ":"s +
                                                     boost::lexical_cast<std::string>(p) +
                                                     k));
    RedisQueue rq(url, 10);

    EXPECT_NO_THROW(rq.push(h));
    EXPECT_NO_THROW(rq.push(k));

    EXPECT_EQ(h,
              rq.pop<std::string>());
    EXPECT_EQ(k,
              rq.pop<std::string>());
}

}
