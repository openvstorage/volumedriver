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

#include <youtils/Assert.h>

#include "../ClusterCache.h"
#include "../ClusterCacheEntry.h"
#include "../ClusterCacheMap.h"

#include "ExGTest.h"

#include <algorithm>

#include <youtils/SourceOfUncertainty.h>

namespace volumedrivertest
{

using namespace volumedriver;
namespace bi = boost::intrusive;
namespace yt = youtils;

class ClusterCacheMapTest
    : public testing::TestWithParam<VolumeDriverTestConfig>
{
protected:
    std::unique_ptr<ClusterCacheEntry>
    random_entry()
    {
        const uint64_t max = std::numeric_limits<uint64_t>::max();
        return std::make_unique<ClusterCacheEntry>(ClusterCacheHandle(sou_(max)),
                                                   sou_(max));
    }

    yt::SourceOfUncertainty sou_;

    TODO("AR: can this be simplified - not only here but also in ClusterCache.h?");
    using SListClusterCacheEntryValueTraits = SListNodeValueTraits<ClusterCacheEntry>;
    using SListClusterCacheEntryValueTraitsOption =
        bi::value_traits<SListClusterCacheEntryValueTraits>;
    using ClusterCacheMapType = ClusterCacheMap<ClusterCacheEntry,
                                                SListClusterCacheEntryValueTraitsOption>;
};

TEST_F(ClusterCacheMapTest, test0)
{
    ClusterCacheMapType map;

    map.resize(0);

    ASSERT_EQ(0U, map.entries());
    ASSERT_EQ(1U, map.spine_size());

    const std::map<uint64_t, uint64_t>& stats = map.stats();

    for (const auto& val : stats)
    {
        ASSERT_EQ(0U,
                  val.first);
        ASSERT_EQ(1U,
                  val.second);
    }

    std::vector<std::unique_ptr<ClusterCacheEntry>> entries;
    entries.reserve(1024);

    for(uint64_t i = 0; i < 1024; ++i)
    {
        entries.emplace_back(random_entry());
        if(map.find(entries.back()->key) == 0)
        {
            map.insert(*entries.back());
            ASSERT_EQ(entries.back().get(),
                      map.insert_checked(*entries.back()));
        }
    }

    ASSERT_EQ(1024U, map.entries());
    ASSERT_EQ(1U, map.spine_size());

    for (const auto& val : stats)
    {
        ASSERT_TRUE((val.first == 1024 and val.second == 1) or
                    (val.second == 0));
    }

    for (const auto& e : entries)
    {
        ClusterCacheEntry* e2 = map.find(e->key);
        ASSERT_EQ(e.get(),
                  e2);
    }
}

TEST_F(ClusterCacheMapTest, test1)
{
    // #define PRINT_OUT
    uint32_t val = sou_(0, 10);
    ClusterCacheMapType map;

    map.resize(val + 10);
    const uint64_t size = 1 << (val+10);
#ifdef PRINT_OUT
    std::cout << "Size of the map " << size << std::endl;
#endif
    std::vector<std::unique_ptr<ClusterCacheEntry>> entries;

    ASSERT_EQ(0U, map.entries());
    ASSERT_EQ(size, map.spine_size());

    const std::map<uint64_t, uint64_t>& stats = map.stats();

    for (const auto& val : stats)
    {
        ASSERT_EQ(0U,
                  val.first);
        ASSERT_EQ(size,
                  val.second);
    }

    for(uint64_t i = 0; i < 1024*1024; ++i)
    {
        entries.emplace_back(random_entry());
        map.insert(*entries.back());
    }

    for (const auto& e : entries)
    {
        ClusterCacheEntry* e2 = map.find(e->key);
        ASSERT_EQ(e.get(),
                  e2);
    }

    uint64_t res = 0;
    uint64_t bins = 0;

    for (const auto& val : stats)
    {
        res += val.first * val.second;
        bins += val.second;

#ifdef PRINT_OUT
        if(val.second > 0)
        {
            std::cout << val.second << " entries with " << val.first << " length." << std::endl;
        }
#endif
    }

    ASSERT_EQ(1024U * 1024U,
              res);

#ifdef PRINT_OUT
    std::cout << "average length " << (double)res / (double) bins;
#endif

}

TEST_F(ClusterCacheMapTest, resize)
{
    ClusterCacheMapType map;

    map.resize(0);
    ASSERT_EQ(0U,
              map.entries());
    ASSERT_EQ(1U,
              map.spine_size());

    const size_t count = 4;

    ASSERT_LT(0U,
              count);
    ASSERT_EQ(0U,
              count % 2);

    std::vector<std::unique_ptr<ClusterCacheEntry>> entries;
    entries.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        entries.emplace_back(random_entry());
        map.insert(*entries.back());
    }

    map.resize(1,
               count);

    ASSERT_EQ(count,
              map.spine_size());

    auto check([&]()
               {
                   ASSERT_EQ(count,
                             map.entries());

                   for (const auto& e : entries)
                   {
                       ClusterCacheEntry* f = map.find(e->key);
                       ASSERT_EQ(e.get(),
                                 f);
                   }
               });

    check();

    map.resize(2,
               count);

    ASSERT_EQ(count / 2,
              map.spine_size());

    check();
}

}

// Local Variables: **
// mode: c++ **
// End: **
