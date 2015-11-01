// Copyright 2015 iNuron NV
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

#include "../StrongTypedPath.h"
#include "../TestBase.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

STRONG_TYPED_PATH(youtilstest, TestPathType);

namespace youtilstest
{

class StrongTypedPathTest
    : public TestBase
{};

TEST_F(StrongTypedPathTest, path_ops)
{
    TestPathType t("foo");
    const fs::path p("bar");

    EXPECT_TRUE(fs::path("bar/foo") == p / t) << p / t;
    EXPECT_TRUE(TestPathType("foo/bar") == t / p) << t / p;
}

TEST_F(StrongTypedPathTest, conversion)
{
    auto fun([](const TestPathType& s)
             {
                 EXPECT_TRUE(true) << s;
             });

    const TestPathType t("foo");
    fun(t);

    // this one should not compile
#if 0
    const fs::path p("path");
    fun(p);
#endif
}

TEST_F(StrongTypedPathTest, ops)
{
    const fs::path p("foo");
    const TestPathType t(p);

    EXPECT_EQ(p, t.path());
    EXPECT_EQ(p.string(), t.str());

    EXPECT_EQ(boost::lexical_cast<std::string>(p),
              boost::lexical_cast<std::string>(t));
}

TEST_F(StrongTypedPathTest, serialization)
{
    typedef boost::archive::text_iarchive iarchive_type;
    typedef boost::archive::text_oarchive oarchive_type;

    TestPathType t("foo");
    std::stringstream ss;

    {
        oarchive_type oa(ss);
        oa << t;
    }

    TestPathType s;
    iarchive_type ia(ss);
    ia >> s;

    EXPECT_EQ(t, s);
}

}
