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

#include <algorithm>
#include "../TestBase.h"
#include "../UUID.h"
#include "../FileUtils.h"
#include <boost/mpl/assert.hpp>
#include <boost/serialization/wrapper.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/scope_exit.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/serialization/list.hpp>
#include <algorithm>

namespace youtilstest
{
using namespace youtils;
namespace bu = boost::uuids;

class UUIDTest : public TestBase
{

};

TEST_F(UUIDTest, test1)
{
    std::vector<UUID> uuids;

    for(int i = 0; i < 1000; i++)
    {
        UUID uuid;
        std::vector<UUID>::iterator p = std::find(uuids.begin(), uuids.end(), uuid);
        ASSERT_TRUE(p == uuids.end());
        uuids.push_back(uuid);
    }
}


TEST_F(UUIDTest, test2)
{
    for(int i = 0; i < 100; i++)
    {
        UUID uuid;
        UUID uuid2 = uuid;
        ASSERT_TRUE(uuid == uuid2);
        UUID uuid3(uuid);
        ASSERT_TRUE(uuid3 == uuid);
    }
}

TEST_F(UUIDTest, test3)
{
    UUID uuid;
    std::string s = uuid.str();
    ASSERT_TRUE(s.size() == 36);
    UUID uuid2(s);
    ASSERT_TRUE(uuid2 == uuid);
}

TEST_F(UUIDTest, test4)
{
    UUID uuid = UUID::NullUUID();
    ASSERT_TRUE(uuid.isNull());

    const UUID& uuid1 = UUID::NullUUID();
    ASSERT_TRUE(uuid.isNull());
    ASSERT_TRUE(uuid == uuid1);

    UUID uuid2(uuid1);
    ASSERT_TRUE(uuid2.isNull());
    ASSERT_TRUE(uuid2 == uuid1);

    UUID uuid3 = uuid1;
    ASSERT_TRUE(uuid3.isNull());
    ASSERT_TRUE(uuid3 == uuid1);

    std::string s = uuid.str();
    ASSERT_TRUE(s.size() == 36);
    UUID uuid4(s);
    ASSERT_TRUE(uuid4.isNull());
    ASSERT_TRUE(uuid4 == uuid);


}

TEST_F(UUIDTest, test5)
{
    UUID uuid;
    const fs::path tmpfile = getTempPath("serialization.txt");
    ALWAYS_CLEANUP_FILE(tmpfile);

    {

        fs::ofstream ofs(tmpfile);
        boost::archive::xml_oarchive oa(ofs);
        oa << BOOST_SERIALIZATION_NVP(uuid);

    }

    UUID uuid2;

    fs::ifstream ifs(tmpfile);
    boost::archive::xml_iarchive ia(ifs);
    ia >>BOOST_SERIALIZATION_NVP(uuid2);


    ASSERT_TRUE(uuid == uuid2);
}

TEST_F(UUIDTest, test6)
{
    std::list<UUID> vec1(20);

    const fs::path tmpfile = getTempPath("serialization.txt");
    ALWAYS_CLEANUP_FILE(tmpfile);

    {

        fs::ofstream ofs(tmpfile);
        boost::archive::xml_oarchive oa(ofs);
        oa << BOOST_SERIALIZATION_NVP(vec1);
    }

    std::list<UUID> vec2;

    fs::ifstream ifs(tmpfile);
    boost::archive::xml_iarchive ia(ifs);
    ia >> BOOST_SERIALIZATION_NVP(vec2);

    ASSERT_TRUE(vec1 == vec2);
}

TEST_F(UUIDTest, cast)
{
    const UUID uuid;
    bu::uuid buuid = static_cast<const bu::uuid&>(uuid);

    ASSERT_EQ(sizeof(buuid),
              uuid.size());

    ASSERT_EQ(0,
              memcmp(uuid.data(),
                     buuid.data,
                     uuid.size()));
}

}

// Local Variables: **
// End: **
