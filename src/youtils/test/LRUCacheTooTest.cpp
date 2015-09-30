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

#include "../LRUCacheToo.h"
#include "../TestBase.h"

#include <boost/lexical_cast.hpp>

namespace youtilstest
{

namespace yt = youtils;

class LRUCacheTooTest
    : public TestBase
{
protected:
    using LRUCache = yt::LRUCacheToo<uint64_t, std::string>;
};

TEST_F(LRUCacheTooTest, capacity)
{
    EXPECT_THROW(LRUCache("Fred",
                          0),
                 std::exception);

    EXPECT_NO_THROW(LRUCache("Ethel",
                             1));
}

TEST_F(LRUCacheTooTest, basics)
{
    LRUCache cache("Alice",
                   1);

    const uint64_t k = 1;
    const auto v(boost::lexical_cast<std::string>(k));

    ASSERT_EQ(1U, cache.capacity());
    ASSERT_EQ(0U, cache.size());

    ASSERT_TRUE(cache.keys().empty());
    ASSERT_EQ(boost::none,
              cache.find(k));

    cache.insert(k,
                 v);

    const auto r(cache.find(k));
    ASSERT_TRUE(r != boost::none);
    ASSERT_EQ(v,
              *r);

    ASSERT_EQ(1U, cache.size());

    const auto keys(cache.keys());
    ASSERT_EQ(1U, keys.size());
    ASSERT_EQ(k,
              keys[0]);

    cache.erase(keys[0]);
    ASSERT_EQ(0U,
              cache.size());
}

TEST_F(LRUCacheTooTest, lru)
{
    const size_t capacity = 7;
    LRUCache cache("Bob",
                   capacity);

    for (size_t i = 0; i < capacity; ++i)
    {
        cache.insert(i, boost::lexical_cast<std::string>(i));
    }

    ASSERT_EQ(capacity, cache.size());
    cache.insert(capacity,
                 boost::lexical_cast<std::string>(capacity));

    ASSERT_EQ(boost::none,
              cache.find(0));

    for (size_t i = capacity; i > 0; --i)
    {
        const auto r(cache.find(i));
        ASSERT_NE(boost::none,
                  r);
        ASSERT_EQ(boost::lexical_cast<std::string>(i),
                  *r);
    }

    cache.insert(capacity + 1,
                 boost::lexical_cast<std::string>(capacity + 1));

    ASSERT_EQ(boost::none,
              cache.find(capacity));

    const auto keyv(cache.keys());
    std::set<uint64_t> keys(keyv.begin(),
                            keyv.end());

    ASSERT_EQ(capacity, keys.size());

    for (size_t i = 1; i < capacity; ++i)
    {
        ASSERT_TRUE(keys.find(i) != keys.end());
    }

    ASSERT_TRUE(keys.find(capacity + 1) != keys.end());

    cache.clear();
    ASSERT_TRUE(cache.empty());
    ASSERT_TRUE(cache.keys().empty());
}

// OVS-2301 brought this up: updates did not work.
TEST_F(LRUCacheTooTest, update)
{
    const size_t capacity = 7;
    LRUCache cache("Henry",
                   capacity);

    const uint64_t key = 5;
    const std::string before(boost::lexical_cast<std::string>(key));

    cache.insert(key,
                 before);

    ASSERT_EQ(before,
              cache.find(key));

    const std::string after("vijf");

    cache.insert(key,
                 after);

    ASSERT_EQ(after,
              cache.find(key));
}

}
