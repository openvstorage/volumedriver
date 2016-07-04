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

#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

#include <boost/lexical_cast.hpp>

#include <gtest/gtest.h>
#include "../DimensionedValue.h"
#include "algorithm"

namespace youtilstest
{
using namespace youtils;

class DimensionedValueTest : public testing::Test
{};

bool
CloseEnough(unsigned long long first,
            unsigned long long second)
{
    return (std::max(first, second) - std::min(first,second) <2ULL);
}


void
testRoundTripToFixPoint(DimensionedValue& x)
{
    DimensionedValue previous (x);

    DimensionedValue current(x);
    uint32_t loops = 0;
    do {
        previous = current;
        current = DimensionedValue(previous.toString());
        EXPECT_EQ(x.getBytes(), current.getBytes());
        loops++;
    } while (current.toString() != previous.toString() and loops < 3);

    EXPECT_EQ(current.toString(), previous.toString());
}


TEST_F(DimensionedValueTest, fancyconstructor)
{
    DimensionedValue v1(1024);
    EXPECT_TRUE(v1.toString() == "1KiB");
    EXPECT_TRUE(v1.getBytes() == 1024);

    DimensionedValue v2(1025);
    EXPECT_TRUE(v2.toString() == "1025B");
    EXPECT_TRUE(v2.getBytes() == 1025);

    DimensionedValue v3(1025*1024);
    EXPECT_TRUE(v3.toString() == "1025KiB");
    EXPECT_TRUE(v3.getBytes() == 1025*1024);

    DimensionedValue v4(1000*1024);
    EXPECT_TRUE(v4.toString() == "1000KiB");
    EXPECT_TRUE(v4.getBytes() == 1000*1024);

    DimensionedValue v5(1000*1023);
    EXPECT_TRUE(v5.toString() == "1023KB");
    EXPECT_TRUE(v5.getBytes() == 1000*1023);
}

TEST_F(DimensionedValueTest, test)
{
    const std::string size1("10 GiB");
    DimensionedValue S1(size1);
    ASSERT_PRED2(CloseEnough, S1.getBytes(),  10737418240ULL );
    testRoundTripToFixPoint(S1);

    const std::string size2("9.32 TiB");
    EXPECT_THROW(DimensionedValue S2(size2), NoParse);

    DimensionedValue S2(18446744073709551615ULL);
    testRoundTripToFixPoint(S2);

    EXPECT_THROW(DimensionedValue S3("18446744073709551617B"), NoParse);

    EXPECT_THROW(DimensionedValue S3("1844674407370 GiB"), NoParse);

    EXPECT_THROW(DimensionedValue S3("184467 PiB"), NoParse);
    EXPECT_THROW(DimensionedValue S3("184467 PB"), NoParse);
    DimensionedValue S17("18445 PB");
    testRoundTripToFixPoint(S17);

    EXPECT_THROW(DimensionedValue S3("18445 PiB"), NoParse);


    const std::string size3("Tib");
    EXPECT_THROW(DimensionedValue S3(size3), NoParse);


    const std::string size4("1029");
    EXPECT_THROW(DimensionedValue S4(size4), NoParse);


    const std::string size5("20,4 BLA BLA");
    EXPECT_THROW(DimensionedValue S5(size5), NoParse);

    const std::string size6(" BLA 10.56 Gib");
    EXPECT_THROW(DimensionedValue S6(size5), NoParse);

    const std::string size7("   23       TB      ");
    DimensionedValue S7(size7);
    testRoundTripToFixPoint(S7);

    const std::string size8("0xABC.DD TB");
    EXPECT_THROW(DimensionedValue S8(size8), NoParse);


    const std::string size9("33KB");
    DimensionedValue S9(size9);
    ASSERT_PRED2(CloseEnough,S9.getBytes(),33000ULL);
    testRoundTripToFixPoint(S9);

    const std::string size11("25GiB");
    DimensionedValue S111(size11);
    EXPECT_EQ(S111.getBytes(), 26843545600ULL);
    testRoundTripToFixPoint(S111);


    const std::string unlimited("UNLIMITED");
    DimensionedValue S13(unlimited);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), S13.getBytes());
    EXPECT_EQ(S13.toString(), unlimited);
    testRoundTripToFixPoint(S13);
}

TEST_F(DimensionedValueTest, BigOneOne)
{
    const uint64_t i = 0xfffffffffffffffeULL;
    DimensionedValue v(i);
    EXPECT_EQ(i, v.getBytes());
}

TEST_F(DimensionedValueTest, BigOneTwo)
{
    const uint64_t i = 0xfffffffffffffffeULL;
    const std::string s(boost::lexical_cast<std::string, uint64_t>(i) + "B");
    DimensionedValue v(i);
    EXPECT_EQ(i, v.getBytes()) << "string: " << s << ", expected bytes: " << i;
}

TEST_F(DimensionedValueTest, lexical_cast)
{
    const uint64_t i = 0xfeeddeadbeefcafeULL;
    DimensionedValue v(i);

    EXPECT_EQ(v.toString(),
              boost::lexical_cast<std::string>(v));

    EXPECT_EQ(v,
              boost::lexical_cast<DimensionedValue>(v.toString()));
}

TEST_F(DimensionedValueTest, successful_streamin)
{
    const DimensionedValue one_hundred_mib("100MiB");
    std::stringstream ss;
    ss << one_hundred_mib;

    DimensionedValue v;
    ss >> v;

    ASSERT_EQ(one_hundred_mib,
              v);
}

TEST_F(DimensionedValueTest, streamin_garbage)
{
    const std::string one_mega_ton("1Mt");
    std::stringstream ss;

    ss << one_mega_ton;

    DimensionedValue v;
    ss >> v;

    ASSERT_FALSE(ss);
}

TEST_F(DimensionedValueTest, equality)
{
    const DimensionedValue a("1MiB");
    const DimensionedValue b("1024KiB");
    const DimensionedValue c("1MiB");

    ASSERT_EQ(a.getBytes(),
              b.getBytes());

    ASSERT_EQ(a.getBytes(),
              c.getBytes());

    ASSERT_EQ(a, c);
    ASSERT_NE(a, b);

    ASSERT_EQ(a.toString(),
              c.toString());

    ASSERT_NE(a.toString(),
              b.toString());

    ASSERT_EQ(a.getString(Prefix::KiB),
              b.getString(Prefix::KiB));

    ASSERT_EQ(a.getString(Prefix::B),
              c.getString(Prefix::B));
}

}

// Local Variables: **
// mode: c++ **
// End: **
