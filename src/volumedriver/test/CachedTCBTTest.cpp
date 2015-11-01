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

#include "ExGTest.h"
#include "TCBTMetaDataStore.h"

namespace volumedrivertesting
{
using namespace volumedriver;

class CachedTCBTTest : public volumedriver::ExGTest
{
protected:

    CachedTCBTTest()
        : directory_(getTempPath("CachedTCBTTest"))
    {

    }
    DECLARE_LOGGER("CachedTCBTTest");


    void SetUp()
    {
        fs::remove_all(directory_);
        fs::create_directories(directory_);
    }

    void TearDown()
    {
        fs::remove_all(directory_);
    }

    //   typedef struct TCBTMetaDataPageTraits<11,true, true>::Page Page;
    typedef struct
    // TCBTMetaDataPageTraits<11, true, true>::
    CachedTCBT C;

    const fs::path directory_;

};

TEST_F(CachedTCBTTest, test1)
{
    fs::create_directories(directory_ / "v1");
    C cache1(directory_ / "v1",1024, true);
    fs::create_directories(directory_ / "v2");
    C cache2(directory_ / "v2",1024, true);

    EXPECT_TRUE(cache1.compare(cache2));
    EXPECT_TRUE(cache2.compare(cache1));

}

TEST_F(CachedTCBTTest, test2)
{
    /*
       IMMANUEL
    fs::create_directories(directory_ / "v1");
    C cache1(directory_ / "v1",C::maxEntriesDefault_);
    fs::create_directories(directory_ / "v2");
    C cache2(directory_ / "v2",C::maxEntriesDefault_);
    cache1.write(1,ClusterLocation(10,23,SCOCloneID(12)));

    EXPECT_FALSE(cache1.compare(cache2));
    EXPECT_FALSE(cache2.compare(cache1));
    cache2.write(1,ClusterLocation(10,23,SCOCloneID(12)));
    EXPECT_TRUE(cache1.compare(cache2));
    EXPECT_TRUE(cache2.compare(cache1));
    cache2.write(20,ClusterLocation(10,23,SCOCloneID(12)));
    EXPECT_FALSE(cache1.compare(cache2));
    EXPECT_FALSE(cache2.compare(cache1));

    cache1.write(20,ClusterLocation(10,23,SCOCloneID(12)));
    EXPECT_TRUE(cache1.compare(cache2));
    EXPECT_TRUE(cache2.compare(cache1));

    cache1.write(20000,ClusterLocation(10,23,SCOCloneID(12)));
    EXPECT_FALSE(cache1.compare(cache2));
    EXPECT_FALSE(cache2.compare(cache1));
    cache2.write(40000,ClusterLocation(10,23,SCOCloneID(12)));
    EXPECT_FALSE(cache1.compare(cache2));
    EXPECT_FALSE(cache2.compare(cache1));
    cache2.write(20000,ClusterLocation(10,23,SCOCloneID(12)));
    EXPECT_FALSE(cache1.compare(cache2));
    EXPECT_FALSE(cache2.compare(cache1));
    cache1.write(40000,ClusterLocation(10,23,SCOCloneID(12)));
    EXPECT_TRUE(cache1.compare(cache2));
    EXPECT_TRUE(cache2.compare(cache1));
    */
}


}

// Local Variables: **
// mode: c++ **
// End: **
