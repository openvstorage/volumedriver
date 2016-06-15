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

#include "../StrongTypedString.h"
#include "../TestBase.h"

#include <functional>
#include <unordered_set>

STRONG_TYPED_STRING(youtilstest, TestStringType);

namespace youtilstest
{

class StrongTypedStringTest
    : public TestBase
{};

TEST_F(StrongTypedStringTest, hash)
{
    TestStringType s1("foo");
    TestStringType s2(s1);
    TestStringType s3("bar");

    std::hash<TestStringType> h;

    EXPECT_EQ(h(s1), h(s2));
    EXPECT_NE(h(s1), h(s3));

    std::unordered_set<TestStringType> set;
    EXPECT_TRUE(set.insert(s1).second);
    EXPECT_FALSE(set.insert(s2).second);

    EXPECT_TRUE(set.insert(s3).second);

    EXPECT_TRUE(set.find(s1) != set.end());
    EXPECT_TRUE(set.find(s3) != set.end());
}

}
