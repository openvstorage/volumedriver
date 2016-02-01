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

#include "../RedisQueue.h"
#include "../TestBase.h"

#include <boost/lexical_cast.hpp>

namespace youtilstest
{
using namespace youtils;
using namespace std::literals::string_literals;

class RedisQueueTest
    : public TestBase
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
