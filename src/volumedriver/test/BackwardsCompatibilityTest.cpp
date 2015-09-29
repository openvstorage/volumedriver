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

#include "ExGTest.h"

#include "../Entry.h"
#include "../SCOAccessData.h"
#include "../SnapshotPersistor.h"
#include "../TLogReader.h"

#include <sstream>

#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>

#include <youtils/FileUtils.h>

#include <backend/Lock.h>

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
        ASSERT_NO_THROW(backend::Lock lock(str));
        backend::Lock lock(str);
        EXPECT_TRUE(lock.hasLock);
        EXPECT_EQ(lock.session_timeout_, boost::posix_time::seconds(42));
        EXPECT_EQ(lock.interrupt_timeout_, boost::posix_time::seconds(10));
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
    ASSERT_EQ(sizeof(mem_tlog) % sizeof(Entry), 0);

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
    EXPECT_EQ(e->clusterLocation().number(), 1);
    EXPECT_TRUE(e->clusterLocation().version() ==  SCOVersion(1));
    EXPECT_TRUE(e->clusterLocation().cloneID() == SCOCloneID(0));
    EXPECT_EQ(e->clusterLocation().offset(), 0);
    EXPECT_EQ(e->clusterAddress(), 0);

    e = t.nextLocation();
    ASSERT_TRUE(e);
    EXPECT_EQ(e->clusterLocation().number(), 0xffffffff);
    EXPECT_TRUE(e->clusterLocation().version() == SCOVersion(0));
    EXPECT_TRUE(e->clusterLocation().cloneID() == SCOCloneID(1));
    EXPECT_EQ(e->clusterLocation().offset(), 0xFF);
    EXPECT_EQ(e->clusterAddress(), 0xFF);

    e = t.nextLocation();
    ASSERT_TRUE(e);
    EXPECT_EQ(e->clusterLocation().number(), 0xff0000);
    EXPECT_TRUE(e->clusterLocation().version() == SCOVersion(0xff));
    EXPECT_TRUE(e->clusterLocation().cloneID() == SCOCloneID(0xff));
    EXPECT_EQ(e->clusterLocation().offset(), 0xffff);
    EXPECT_EQ(e->clusterAddress(), 0xffffffff);

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
    ASSERT_EQ(ad.getVector().size(), 0);
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
    ASSERT_EQ(ad.getVector().size(), 2);
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
