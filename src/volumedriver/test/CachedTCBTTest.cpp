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

#include "ExGTest.h"
#include "TCBTMetaDataStore.h"

namespace volumedrivertesting
{
using namespace volumedriver;

class CachedTCBTTest : public volumedriver::ExGTest
{
protected:

    CachedTCBTTest()
        : directory_(yt::FileUtils::temp_path("CachedTCBTTest"))
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
