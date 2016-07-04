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

#include <algorithm>
#include <gtest/gtest.h>
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

class UUIDTest : public testing::Test
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
    const fs::path tmpfile(FileUtils::create_temp_file(FileUtils::temp_path(),
                                                       "serialization.txt"));
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

    const fs::path tmpfile(FileUtils::create_temp_file(FileUtils::temp_path(),
                                                       "serialization.txt"));

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
