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
