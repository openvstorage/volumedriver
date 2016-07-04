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

#include <filedriver/ExtentId.h>

#include <youtils/Assert.h>
#include <gtest/gtest.h>

namespace filedrivertest
{

namespace fd = filedriver;

class ExtentIdTest
    : public testing::Test
{
protected:
    const std::string separator = fd::ExtentId::separator_;
    const size_t suffix_size = fd::ExtentId::suffix_size_;
};

TEST_F(ExtentIdTest, roundtrip)
{
    const fd::ContainerId cid("container");
    const uint32_t off(42);

    const fd::ExtentId eid1(cid, off);
    ASSERT_EQ(cid, eid1.container_id);
    ASSERT_EQ(off, eid1.offset);

    const fd::ExtentId eid2(eid1.str());
    ASSERT_EQ(cid, eid2.container_id);
    ASSERT_EQ(off, eid2.offset);

    ASSERT_EQ(eid1, eid2);
}

TEST_F(ExtentIdTest, malformed)
{
    const fd::ContainerId cid("container");
    const uint32_t off(23);

    const fd::ExtentId eid(cid, off);

    const std::string eidstr(eid.str());

    // suffix too short
    EXPECT_THROW(fd::ExtentId(eidstr.substr(0, eidstr.size() - 2)),
                 fd::ExtentId::NotAnExtentId);

    // suffix too long
    EXPECT_THROW(fd::ExtentId(eidstr + "0"),
                 fd::ExtentId::NotAnExtentId);

    // wrong separator
    TODO("AR: figure out what's wrong here");
    // This is really curious:
    //
    // EXPECT_THROW(fd::ExtentId(str),
    //              fd::ExtentId::NotAnExtentId);
    //
    // yields a compiler error:
    //
    // /home/arne/Projects/CloudFounders/3.6/volumedriver-core/src/filedriver/test/ExtentIdTest.cpp:56:34: error: no matching function for call to 'filedriver::ExtentId::ExtentId()'
    //  EXPECT_THROW(fd::ExtentId(str),
    // ...
    //
    // . The nonsensical lambda is to work around that.
    auto fun([&]
             {
                 std::string str(eidstr);
                 const char sep = '*';
                 ASSERT_NE(sep, separator[0]);
                 str[str.find_last_of(separator)] = sep;

                 fd::ExtentId eid(str);
             });

    EXPECT_THROW(fun(),
                 fd::ExtentId::NotAnExtentId);
}

}
