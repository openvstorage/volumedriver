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
