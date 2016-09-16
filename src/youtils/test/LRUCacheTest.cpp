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

#include "../LRUCache.h"
#include <gtest/gtest.h>

#include <boost/bimap/unordered_set_of.hpp>
#include <boost/lexical_cast.hpp>

namespace youtilstest
{

namespace yt = youtils;

class LRUCacheTest
    : public testing::Test
{
protected:
    typedef yt::LRUCache<uint64_t, std::string, boost::bimaps::unordered_set_of> LRUCache;
};

TEST_F(LRUCacheTest, capacity)
{
    EXPECT_THROW(LRUCache("cache without capacity",
                          0,
                          [](const LRUCache::Key&, LRUCache::Value&) {}),
                 std::exception);

    LRUCache("cache with capacity",
             1,
             [](const LRUCache::Key&, LRUCache::Value&) {});
}

TEST_F(LRUCacheTest, basics)
{
    std::map<LRUCache::Key, LRUCache::Value> map;

    auto fetch([&](const LRUCache::Key& k) -> LRUCache::UniqueValuePtr
               {
                   auto it = map.find(k);
                   if (it == map.end())
                   {
                       return nullptr;
                   }
                   else
                   {
                       return LRUCache::UniqueValuePtr(new LRUCache::Value(it->second));
                   }
               });

    auto evict([&](const LRUCache::Key& k, LRUCache::Value&)
               {
                   map.erase(k);
               });

    const uint64_t capacity = 2;
    std::vector<LRUCache::Key> keys(capacity + 1);

    for (size_t k = 0; k < keys.size(); ++k)
    {
        keys[k] = k;
    }

    LRUCache cache("simple cache",
                   capacity,
                   std::move(evict));

    for (const auto& k : keys)
    {
        EXPECT_TRUE(nullptr == cache.find(k, fetch));
        EXPECT_TRUE(cache.keys().empty());
    }

    for (const auto& k : keys)
    {
        map[k] = boost::lexical_cast<LRUCache::Value>(k);
    }

    for (const auto& k : keys)
    {
        LRUCache::ValuePtr v(cache.find(k, fetch));
        EXPECT_EQ(boost::lexical_cast<LRUCache::Value>(k), *v);
    }

    const std::list<LRUCache::Key> list(cache.keys());
    for (const auto& l : list)
    {
        EXPECT_NE(keys[0], l);
    }
}

TEST_F(LRUCacheTest, insert_and_erase)
{
    std::map<LRUCache::Key, LRUCache::Value> evicted;

    LRUCache cache("simple cache",
                   1,
                   [&](const LRUCache::Key& k, LRUCache::Value& v)
                   {
                       const auto res(evicted.insert(std::make_pair(k, v)));
                       EXPECT_TRUE(res.second);
                   });

    const uint64_t k0 = 0;
    const std::string v0(boost::lexical_cast<LRUCache::Value>(k0));

    {
        LRUCache::ValuePtr
            val(cache.find(k0,
                           [&](const LRUCache::Key&)
                           {
                               return std::unique_ptr<LRUCache::Value>(new std::string(v0));
                           }));
        EXPECT_EQ(v0, *val);
    }

    {
        LRUCache::ValuePtr
            val(cache.find(k0,
                           [&](const LRUCache::Key&)
                           {
                               return std::unique_ptr<LRUCache::Value>(new std::string("foo"));
                           }));
        EXPECT_EQ(v0, *val);
    }

    {
        LRUCache::ValuePtr val(cache.find(k0,
                                          [](const LRUCache::Key&)
                                          {
                                              return nullptr;
                                          }));
        EXPECT_EQ(v0, *val);
    }

    const uint64_t k1 = 1;
    const std::string v1(boost::lexical_cast<LRUCache::Value>(k1));

    {
        LRUCache::ValuePtr
            val(cache.find(k1,
                           [&](const LRUCache::Key&)
                           {
                               return std::unique_ptr<LRUCache::Value>(new std::string(v1));
                           }));
        EXPECT_EQ(v1, *val);
    }

    EXPECT_FALSE(cache.erase(k0));

    {
        LRUCache::ValuePtr val(cache.find(k1, [](const LRUCache::Key&){ return nullptr; }));
        EXPECT_EQ(v1, *val);
    }

    EXPECT_EQ(1U, cache.keys().size());
    EXPECT_EQ(k1, cache.keys().front());
    EXPECT_EQ(1U, evicted.size());

    {
        auto it = evicted.find(k0);
        ASSERT_TRUE(it != evicted.end());
        EXPECT_EQ(v0, it->second);
    }

    EXPECT_TRUE(cache.erase(k1));
    EXPECT_FALSE(cache.erase(k1));
    EXPECT_TRUE(cache.find(k1, [](const LRUCache::Key&){ return nullptr; }) == nullptr);

    EXPECT_TRUE(cache.keys().empty());
    EXPECT_EQ(2U, evicted.size());

    auto it = evicted.find(k0);
    ASSERT_TRUE(it != evicted.end());
    EXPECT_EQ(v0, it->second);

    it = evicted.find(k1);
    ASSERT_TRUE(it != evicted.end());
    EXPECT_EQ(v1, it->second);
}

TEST_F(LRUCacheTest, business)
{
    LRUCache cache("simple cache",
                   1,
                   [&](const LRUCache::Key&, LRUCache::Value&)
                   {});

    const uint64_t key = 0;
    LRUCache::ValuePtr
        val(cache.find(key,
                       [](const LRUCache::Key& k)
                       {
                           LRUCache::Value v(boost::lexical_cast<LRUCache::Value>(k));
                           return LRUCache::UniqueValuePtr(new LRUCache::Value(v));
                       }));

    EXPECT_THROW(cache.find(1, [](const LRUCache::Key&)
                            {
                                LRUCache::Value v(boost::lexical_cast<LRUCache::Value>("eins"));
                                return LRUCache::UniqueValuePtr(new LRUCache::Value(v));
                            }),
                 LRUCache::BusyCacheException);

    {
        EXPECT_THROW(cache.find(42,
                                [](const LRUCache::Key&)
                                {
                                    return LRUCache::UniqueValuePtr(new LRUCache::Value("fourtytwo"));
                                }),
                     LRUCache::BusyCacheException);
    }

    EXPECT_THROW(cache.erase(key),
                 LRUCache::BusyEntryException);

    EXPECT_EQ(*val, *cache.find(key,
                                [](const LRUCache::Key&)
                                {
                                    return nullptr;
                                }));
}

TEST_F(LRUCacheTest, remove_on_exception)
{
    LRUCache cache("simple cache",
                   1,
                   [&](const LRUCache::Key&, LRUCache::Value&)
                   {});

    const uint64_t key = 0;
    {
        LRUCache::ValuePtr
            val(cache.find(key,
                           [](const LRUCache::Key& k)
                           {
                               LRUCache::Value v(boost::lexical_cast<LRUCache::Value>(k));
                               return LRUCache::UniqueValuePtr(new LRUCache::Value(v));
                           }));

        EXPECT_THROW(cache.find(1, [](const LRUCache::Key&)
                                {
                                    LRUCache::Value v(boost::lexical_cast<LRUCache::Value>("eins"));
                                    return LRUCache::UniqueValuePtr(new LRUCache::Value(v));
                                }),
                     LRUCache::BusyCacheException);
    }

    EXPECT_NO_THROW(cache.erase(key));

    {
        LRUCache::ValuePtr
            val(cache.find(1,
                           [](const LRUCache::Key&)
                           {
                               LRUCache::Value v(boost::lexical_cast<LRUCache::Value>("eins"));
                               return LRUCache::UniqueValuePtr(new LRUCache::Value(v));
                           }));

        EXPECT_EQ(*val, *cache.find(1,
                                    [](const LRUCache::Key&)
                                    {
                                        return nullptr;
                                    }));
    }
}

TEST_F(LRUCacheTest, concurrency)
{
    boost::mutex lock;
    boost::condition_variable cond;
    bool stage1 = false;
    bool stage2 = false;

    LRUCache cache("simple cache",
                   1,
                   [&](const LRUCache::Key&, LRUCache::Value&)
                   {});

    auto fetch([&](const LRUCache::Key& k)
               {
                   boost::unique_lock<decltype(lock)> u(lock);
                   stage1 = true;
                   cond.notify_one();
                   cond.wait(u, [&] { return stage2; });
                   LRUCache::Value v(boost::lexical_cast<LRUCache::Value>(k));
                   return LRUCache::UniqueValuePtr(new LRUCache::Value(v));
               });

    auto f1(std::async(std::launch::async,
                       [&]() -> LRUCache::ValuePtr
                       {
                           return cache.find(0, fetch);
                       }));

    auto f2(std::async(std::launch::async,
                       [&]() -> LRUCache::ValuePtr
                       {
                           return cache.find(0, fetch);
                       }));

    {
        boost::unique_lock<decltype(lock)> u(lock);
        cond.wait(u, [&]{ return stage1; });
        stage2 = true;
        cond.notify_one();
    }

    EXPECT_TRUE(f1.get() == f2.get());
}

TEST_F(LRUCacheTest, resize)
{
    size_t cap = 1;
    LRUCache cache("simple cache",
                   cap,
                   [&](const LRUCache::Key&, LRUCache::Value&){});

    EXPECT_EQ(cap, cache.capacity());
    EXPECT_THROW(cache.capacity(0),
                 std::exception);
    EXPECT_EQ(cap, cache.capacity());

    cap = 17;
    cache.capacity(cap);
    EXPECT_EQ(cap, cache.capacity());

    {
        std::vector<LRUCache::ValuePtr> vs;
        vs.reserve(cap);

        for (uint64_t i = 0; i < cap; ++i)
        {
            auto fn([&](const LRUCache::Key& k)
                    {
                        const std::string v(boost::lexical_cast<LRUCache::Value>(k));
                        return std::unique_ptr<LRUCache::Value>(new LRUCache::Value(v));
                    });

            vs.push_back(cache.find(i, fn));
        }

        EXPECT_EQ(cap, cache.size());
        EXPECT_LT(1U, cache.capacity());

        EXPECT_THROW(cache.capacity(cap - 1),
                     LRUCache::BusyCacheException);

        EXPECT_EQ(cap, cache.capacity());
        EXPECT_EQ(cap, cache.size());
    }

    cap = 7;
    cache.capacity(cap);
    EXPECT_EQ(cap, cache.capacity());
    EXPECT_EQ(cap, cache.size());
}

}
