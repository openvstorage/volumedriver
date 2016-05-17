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

#include "../Entry.h"
#include "../SCOAccessData.h"
#include "../SnapshotPersistor.h"
#include "../TLogReader.h"

#include <sstream>

#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>

#include <youtils/FileUtils.h>

#include <youtils/HeartBeatLock.h>

namespace volumedrivertest
{
using namespace volumedriver;

class BackwardsCompatibilityTest
    : public ExGTest
{
protected:
    void
    check_rest(const char* str)
    {
        ASSERT_NO_THROW(youtils::HeartBeatLock lock(str));
        youtils::HeartBeatLock lock(str);
        EXPECT_TRUE(lock.hasLock());
        EXPECT_EQ(lock.session_timeout(),
                  boost::posix_time::seconds(42));
        EXPECT_EQ(lock.interrupt_timeout(),
                  boost::posix_time::seconds(10));
    }
};

const char* lock_version_zero = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>\n"
    "<!DOCTYPE boost_serialization>\n"
    "<boost_serialization signature=\"serialization::archive\" version=\"9\">\n"
    "<lock class_id=\"0\" tracking_level=\"0\" version=\"0\">\n"
    "	<uuid class_id=\"1\" tracking_level=\"0\" version=\"0\">\n"
    "		<uuid>5c70717f-ee17-460b-87e0-46903fdc7f49</uuid>\n"
    "	</uuid>\n"
    "	<hasLock>1</hasLock>\n"
    "	<counter>0</counter>\n"
    "	<session_timeout_milliseconds>42000</session_timeout_milliseconds>\n"
    "	<interrupt_timeout_milliseconds>10000</interrupt_timeout_milliseconds>\n"
    "	<hostname>firebird</hostname>\n"
    "	<pid>19994</pid>\n"
    "</lock>";

const byte mem_tlog[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 0, 0, 0, 0,
    // Dis part be the weed, mon!
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0xff, 0, 0, 0, 0, 0, 0, 0,
    0xff, 0, 0, 0XFF, 0xFF, 0xFF, 0xFF, 1,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0,
    0xff, 0xff, 0xff, 0, 0, 0xff, 0, 0xff,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    // this one is an invalid entry ...
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0, 0, 0, 0, 0xff, // ... <- as the SCO number is 0

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

TEST_F(BackwardsCompatibilityTest, tlog)
{
    ASSERT_EQ(0U, sizeof(mem_tlog) % sizeof(Entry));

    fs::path tlog = FileUtils::create_temp_file(FileUtils::temp_path(),
                                                "tlog");
    ALWAYS_CLEANUP_FILE(tlog);

    fs::ofstream tlogstream(tlog);
    tlogstream.write((const char*)mem_tlog, sizeof(mem_tlog));
    ASSERT_TRUE(tlogstream.good());
    tlogstream.close();

    TLogReader t(tlog);

    const Entry* e = t.nextLocation();
    ASSERT_TRUE(e);
    EXPECT_EQ(1U, e->clusterLocation().number());
    EXPECT_EQ(SCOVersion(1), e->clusterLocation().version());
    EXPECT_EQ(SCOCloneID(0), e->clusterLocation().cloneID());
    EXPECT_EQ(0U, e->clusterLocation().offset());
    EXPECT_EQ(0U, e->clusterAddress());

    e = t.nextLocation();
    ASSERT_TRUE(e);
    EXPECT_EQ(0xffffffffU, e->clusterLocation().number());
    EXPECT_EQ(SCOVersion(0), e->clusterLocation().version());
    EXPECT_EQ(SCOCloneID(1), e->clusterLocation().cloneID());
    EXPECT_EQ(0xFFU, e->clusterLocation().offset());
    EXPECT_EQ(0xFFU, e->clusterAddress());

    e = t.nextLocation();
    ASSERT_TRUE(e);
    EXPECT_EQ(0xff0000U, e->clusterLocation().number());
    EXPECT_EQ(SCOVersion(0xff), e->clusterLocation().version());
    EXPECT_EQ(SCOCloneID(0xff), e->clusterLocation().cloneID());
    EXPECT_EQ(0xffffU, e->clusterLocation().offset());
    EXPECT_EQ(0xffffffffU, e->clusterAddress());

    e = t.nextLocation();
    ASSERT_FALSE(e);
}

const char sco_access_data_2_0_0 [] =
{
    22, 0, 0, 0, 0, 0, 0, 0, 115, 101,
    114, 105, 97, 108, 105, 122, 97, 116, 105, 111,
    110, 58, 58, 97, 114, 99, 104, 105, 118, 101,
    7, 4, 8, 4, 8, 1, 0, 0, 0, 0,
    0, 10, 0, 0, 0, 0, 0, 0, 0, 110,
    97, 109, 101, 115, 112, 97, 99, 101, 49, 0,
    0, 0, 0, 0, 0, 2, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
    0, -51, -52, 68, 65, 0, 0, 0, 0, 0,
    2, 0, 0, 0, 0, 0, 0, -51, -52,
    100, 65, 115, 112, 97, 99
};

TEST_F(BackwardsCompatibilityTest, scoaccessdata_2_0_0)
{

    std::string s(sco_access_data_2_0_0, sizeof(sco_access_data_2_0_0));

    std::stringstream oss(s);
    SCOAccessDataPersistor::iarchive_type ia(oss);

    SCOAccessData ad((backend::Namespace()));
    ia & ad;
    ASSERT_EQ(0U, ad.getVector().size());
}

const char sco_access_data_def[] =  {
    22, 0, 0, 0, 0, 0, 0, 0, 115, 101,
    114, 105, 97, 108, 105, 122, 97, 116, 105, 111,
    110, 58, 58, 97, 114, 99, 104, 105, 118, 101,
    7, 4, 8, 4, 8, 1, 0, 0, 0, 0,
    1, 10, 0, 0, 0, 0, 0, 0, 0, 110,
    97, 109, 101, 115, 112, 97, 99, 101, 49, 0,
    0, 0, 0, 0, 0, 2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, -51, -52, 68,
    65, 0, 2, 0, 0, 0, 0, -51, -52, 100, 65
};

TEST_F(BackwardsCompatibilityTest, scoaccessdata_def)
{
    std::string s(sco_access_data_def, sizeof(sco_access_data_def));

    std::stringstream oss(s);
    SCOAccessDataPersistor::iarchive_type ia(oss);

    SCOAccessData ad((backend::Namespace()));
    ia & ad;
    ASSERT_EQ(2U, ad.getVector().size());
    typedef const std::pair<SCO, float> fp;
    BOOST_FOREACH(fp& p, ad.getVector())
    {
        EXPECT_TRUE(p.first == SCO(1) or p.first == SCO(2));
        EXPECT_TRUE(p.second == (float)12.3 or p.second == (float)14.3);
        // std::cout << p.first << "--" << p.second << std::endl;
    }
}

TEST_F(BackwardsCompatibilityTest, lock)
{
    check_rest(lock_version_zero);
}

// Local Variables: **
// mode: c++ **
// End: **

}
