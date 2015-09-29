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

#if 0
#include "VolManagerTestSetup.h"
#include "SCOCacheMap.h"

namespace volumedriver
{

class SCOCacheMapTest
    : public VolManagerTestSetup
{
public:
    SCOCacheMapTest()
        : VolManagerTestSetup("SCOCacheMapTest")
    {}

    virtual void SetUp()
    {
        VolManagerTestSetup::SetUp();
        check2TCSanity_ = false;
    }

    virtual void TearDown()
    {
        VolManagerTestSetup::TearDown();
    }

protected:
    DECLARE_LOGGER("SCOCacheMapTest");
};

TEST_P(SCOCacheMapTest, namespaces)
{
    SCOCacheMap& cacheMap = getTwoTierCacheSCOCacheMap();

    EXPECT_TRUE(cacheMap.begin() == cacheMap.end());

    const std::string nspfx("nspace");

    for (size_t i = 0; i < 3; ++i)
    {
        std::stringstream ss;
        ss << nspfx << i;

        EXPECT_FALSE(cacheMap.hasNamespace(ss.str()));
        cacheMap.addNamespace(ss.str());
        EXPECT_TRUE(cacheMap.hasNamespace(ss.str()));

        // adding the same namespace again should succeed
        cacheMap.addNamespace(ss.str());
    }

    for (size_t i = 0; i < 3; ++i)
    {
        std::stringstream ss;
        ss << nspfx << i;

        EXPECT_TRUE(cacheMap.hasNamespace(ss.str()));
        cacheMap.removeNamespace(ss.str());
        EXPECT_FALSE(cacheMap.hasNamespace(ss.str()));

        // removing the same namespace again should succeed
        cacheMap.removeNamespace(ss.str());
        EXPECT_FALSE(cacheMap.hasNamespace(ss.str()));
    }
}

TEST_P(SCOCacheMapTest, iterator)
{
    SCOCacheMap& cacheMap = getTwoTierCacheSCOCacheMap();

    const std::string nspfx("nspace");
    const uint64_t scoSize = 4 << 10;

    size_t nnspace = 3;
    size_t nscos = 7;

    SCOCacheMap::Iterator it = cacheMap.begin();
    EXPECT_TRUE(it == cacheMap.end());

    EXPECT_THROW(++it,
                 std::out_of_range);

    CachedSCOPtr sco;
    EXPECT_THROW((sco = *it),
                 std::out_of_range);

    for (size_t i = 0; i < 3; ++i)
    {
        std::stringstream ss;
        ss << nspfx << i;

        VolManager::get()->get2TC()->addNamespace(ss.str());

        for (SCOName scoName = 1; scoName <= nscos; ++scoName)
        {
            VolManager::get()->get2TC()->createSCO(ss.str(),
                                                   scoName,
                                                   scoSize);
        }
    }

    size_t count = 0;

    for (SCOCacheMap::Iterator it2 = cacheMap.begin();
         it2 != cacheMap.end();
         ++it2)
    {
        ++count;
    }

    EXPECT_EQ(nnspace * nscos, count);

    std::stringstream ss;
    ss << nspfx << 0;

    VolManager::get()->get2TC()->removeNamespace(ss.str());

    count = 0;
    for (SCOCacheMap::Iterator it2 = cacheMap.begin();
         it2 != cacheMap.end();
         ++it2)
    {
        ++count;
    }

    EXPECT_EQ((nnspace - 1) * nscos, count);
    EXPECT_TRUE(cacheMap.end() == cacheMap.find(ss.str(),
                                                1));

    ss.str("");
    ss << nspfx << 1;

    it = cacheMap.find(ss.str(),
                       1);

    SCOCacheMap::Iterator tmp = it;
    ++tmp;
    EXPECT_FALSE(tmp == cacheMap.end());

    cacheMap.erase(it);
    EXPECT_THROW(*it,
                 std::out_of_range);
    EXPECT_THROW(++it,
                 std::out_of_range);

    ss.str("");
    ss << nspfx << 2;

    it = cacheMap.find(ss.str(),
                       1);

    sco = *it;

    EXPECT_EQ(ss.str(), sco->getNamespace());
    EXPECT_EQ(1, sco->getSCOName());

    cacheMap.erase(sco);

    EXPECT_TRUE(cacheMap.end() == cacheMap.find(ss.str(),
                                                1));
}

INSTANTIATE_TEST(SCOCacheMapTest);

}

#endif

// Local Variables: **
// mode: c++ **
// End: **
