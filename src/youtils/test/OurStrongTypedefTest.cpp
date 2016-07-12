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

#include "../OurStrongTypedef.h"
#include <gtest/gtest.h>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

namespace youtilstest
{

namespace ba = boost::archive;
namespace bs = boost::serialization;

class OurStrongTypedefTest
    : public testing::Test
{
protected:
    template<typename T, typename D>
    void
    test_limits()
    {
#define CHECKV(x)                                                       \
        EXPECT_TRUE(std::numeric_limits<T>::x == std::numeric_limits<D>::x)

        CHECKV(is_specialized);
        CHECKV(digits);
        CHECKV(digits10);
        CHECKV(is_signed);
        CHECKV(is_integer);
        CHECKV(is_exact);
        CHECKV(radix);
        CHECKV(min_exponent);
        CHECKV(min_exponent10);
        CHECKV(max_exponent);
        CHECKV(max_exponent10);
        CHECKV(has_infinity);
        CHECKV(has_quiet_NaN);
        CHECKV(has_signaling_NaN);
        CHECKV(has_denorm);
        CHECKV(has_denorm_loss);
        CHECKV(is_iec559);
        CHECKV(is_bounded);
        CHECKV(is_modulo);
        CHECKV(traps);
        CHECKV(tinyness_before);
        CHECKV(round_style);

#undef CHECKV

#define CHECKF(x)                                                       \
        EXPECT_EQ(D(std::numeric_limits<T>::x()), std::numeric_limits<D>::x())

        CHECKF(min);
        CHECKF(max);
        CHECKF(lowest);
        CHECKF(epsilon);
        CHECKF(round_error);
        CHECKF(infinity);
        CHECKF(denorm_min);

        if (std::numeric_limits<T>::is_integer)
        {
            CHECKF(quiet_NaN);
            CHECKF(signaling_NaN);
        }
        else
        {
            EXPECT_TRUE(std::isnan(static_cast<T>(std::numeric_limits<D>::quiet_NaN())));
            EXPECT_TRUE(std::isnan(static_cast<T>(std::numeric_limits<D>::signaling_NaN())));
        }

#undef CHECKF
    }

    template<typename T>
    void
    test_truthiness()
    {
        const T falsy(static_cast<T>(0));
        EXPECT_FALSE(falsy);

        const T truthy(static_cast<T>(1));
        EXPECT_TRUE(truthy);
    }

    template<typename T,
             typename OArchive,
             typename IArchive>
    void
    test_serialization(const T& t)
    {
        std::stringstream ss;

        {
            OArchive oa(ss);
            oa << t;
        }

        T u;
        {
            IArchive ia(ss);
            ia >> u;
        }

        EXPECT_EQ(t, u);
    }
};

}

// Exercise the 16 fundamental arithmetic types specialized in <limits>
// to make sure I didn't goof in the numeric_limits specialization for the
// derived type.


#define TEST_NUMERIC_LIMITS(base, type, testname)               \
                                                                \
    OUR_STRONG_ARITHMETIC_TYPEDEF(base, type, youtilstest);     \
                                                                \
    namespace youtilstest                                       \
    {                                                           \
        TEST_F(OurStrongTypedefTest, testname)                  \
        {                                                       \
            test_limits<base, type>();                          \
        }                                                       \
    }

TEST_NUMERIC_LIMITS(bool, Bool, numeric_limits_bool);
TEST_NUMERIC_LIMITS(char, Char, numeric_limits_char);
TEST_NUMERIC_LIMITS(signed char, SignedChar, numeric_limits_signed_char);
TEST_NUMERIC_LIMITS(unsigned char, UnsignedChar, numeric_limits_unsigned_char);
TEST_NUMERIC_LIMITS(wchar_t, WChar, numeric_limits_wchar);
TEST_NUMERIC_LIMITS(short, Short, numeric_limits_short);
TEST_NUMERIC_LIMITS(unsigned short, UnsignedShort, numeric_limits_unsigned_short);
TEST_NUMERIC_LIMITS(int, Int, numeric_limits_int);
TEST_NUMERIC_LIMITS(unsigned int, UnsignedInt, numeric_limits_unsigned_int);
TEST_NUMERIC_LIMITS(long, Long, numeric_limits_long);
TEST_NUMERIC_LIMITS(unsigned long, UnsignedLong, numeric_limits_unsigned_long);
TEST_NUMERIC_LIMITS(long long, LongLong, numeric_limits_long_long);
TEST_NUMERIC_LIMITS(unsigned long long, UnsignedLongLong, numeric_limits_unsigned_long_long);
TEST_NUMERIC_LIMITS(float, Float, numeric_limits_float);
TEST_NUMERIC_LIMITS(double, Double, numeric_limits_double);
TEST_NUMERIC_LIMITS(long double, LongDouble, numeric_limits_long_double);

#undef TEST_NUMERIC_LIMITS

namespace youtilstest
{

// TODO: reuses the strong typedefs introduced by TEST_NUMERIC_LIMITS -
// which is kinda ugly.
#define TEST_TRUTHINESS(base, type, testname)                   \
    TEST_F(OurStrongTypedefTest, testname)                      \
    {                                                           \
        test_truthiness<type>();                                \
    }

TEST_TRUTHINESS(bool, Bool, thruthiness_bool);
TEST_TRUTHINESS(char, Char, thruthiness_char);
TEST_TRUTHINESS(signed char, SignedChar, thruthiness_signed_char);
TEST_TRUTHINESS(unsigned char, UnsignedChar, thruthiness_unsigned_char);
TEST_TRUTHINESS(wchar_t, WChar, thruthiness_wchar);
TEST_TRUTHINESS(short, Short, thruthiness_short);
TEST_TRUTHINESS(unsigned short, UnsignedShort, thruthiness_unsigned_short);
TEST_TRUTHINESS(int, Int, thruthiness_int);
TEST_TRUTHINESS(unsigned int, UnsignedInt, thruthiness_unsigned_int);
TEST_TRUTHINESS(long, Long, thruthiness_long);
TEST_TRUTHINESS(unsigned long, UnsignedLong, thruthiness_unsigned_long);
TEST_TRUTHINESS(long long, LongLong, thruthiness_long_long);
TEST_TRUTHINESS(unsigned long long, UnsignedLongLong, thruthiness_unsigned_long_long);
TEST_TRUTHINESS(float, Float, thruthiness_float);
TEST_TRUTHINESS(double, Double, thruthiness_double);
TEST_TRUTHINESS(long double, LongDouble, thruthiness_long_double);

#undef TEST_TRUTHINESS

TEST_F(OurStrongTypedefTest, text_serialization)
{
    test_serialization<Float,
                       ba::text_oarchive,
                       ba::text_iarchive>(Float(42.0));
}

TEST_F(OurStrongTypedefTest, binary_serialization)
{
    test_serialization<UnsignedInt,
                       ba::binary_oarchive,
                       ba::binary_iarchive>(UnsignedInt(0xbeefcafe));
}

}
