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

#include <gtest/gtest.h>
#include "../BooleanEnum.h"

#include <sstream>

namespace youtilstest
{

class BooleanEnumTest
    : public testing::Test
{};

BOOLEAN_ENUM(BooleanTest)

TEST_F(BooleanEnumTest, test0)
{
    ASSERT_TRUE(T(BooleanTest::T));
    ASSERT_TRUE(F(BooleanTest::F));
    ASSERT_FALSE(F(BooleanTest::T));
    ASSERT_FALSE(T(BooleanTest::F));

    ASSERT_EQ(BooleanTest::T, BooleanTest::T);
    ASSERT_EQ(BooleanTest::F, BooleanTest::F);
    ASSERT_NE(BooleanTest::T, BooleanTest::F);
}

TEST_F(BooleanEnumTest, test1)
{
    BooleanTest f1 = BooleanTest::F;
    BooleanTest t1 = BooleanTest::T;
    BooleanTest tmp = BooleanTest::T;

    {
        std::stringstream ss;

        ss << f1;
        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        ASSERT_EQ(ss.str(),std::string("BooleanTest::F"));

        ss >> tmp;

        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        ASSERT_EQ(tmp, BooleanTest::F);
        ss >> tmp;
        ASSERT_TRUE(ss.fail());
        ASSERT_TRUE(ss.eof());
        ASSERT_TRUE(ss.bad());
        ASSERT_FALSE(ss.good());
        ASSERT_FALSE(ss);
    }

    {
        std::stringstream ss;

        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        ss << t1;
        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        ASSERT_EQ(ss.str(),std::string("BooleanTest::T"));
        ss >> tmp;
        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        ASSERT_EQ(tmp, BooleanTest::T);
        ss >> tmp;
        ASSERT_TRUE(ss.fail());
        ASSERT_TRUE(ss.eof());
        ASSERT_TRUE(ss.bad());
        ASSERT_FALSE(ss.good());
        ASSERT_FALSE(ss);
    }
}

TEST_F(BooleanEnumTest, test2)
{
    {
        std::stringstream ss("Initial Content");

        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        BooleanTest t;
        ss >> t;

        ASSERT_FALSE(ss.good());
        ASSERT_FALSE(ss.eof());
        ASSERT_TRUE(ss.fail());
        ASSERT_TRUE(ss.bad());
    }

    {
        std::stringstream ss("BooleanTe");

        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.eof());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        BooleanTest t;
        ss >> t;

        ASSERT_FALSE(ss.good());
        ASSERT_TRUE(ss.eof());
        ASSERT_TRUE(ss.fail());
        ASSERT_TRUE(ss.bad());
    }

    {
        std::stringstream ss("BooleanTest::");

        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.eof());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        BooleanTest t;
        ss >> t;

        ASSERT_FALSE(ss.good());
        ASSERT_TRUE(ss.eof());
        ASSERT_TRUE(ss.fail());
        ASSERT_TRUE(ss.bad());
    }

    {
        std::stringstream ss("BooleanTest::K");

        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.eof());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        BooleanTest t;
        ss >> t;

        ASSERT_FALSE(ss.good());
        ASSERT_FALSE(ss.eof());
        ASSERT_TRUE(ss.fail());
        ASSERT_TRUE(ss.bad());
    }

    {
        std::stringstream ss("BooleanTest::Trump");

        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.eof());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        BooleanTest t;
        ss >> t;
        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.eof());
        ASSERT_FALSE(ss.fail());
        ASSERT_FALSE(ss.bad());

        ASSERT_EQ(t, BooleanTest::T);
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
