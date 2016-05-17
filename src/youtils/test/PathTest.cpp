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

#include "../Logging.h"
#include "../TestBase.h"

#include <boost/filesystem.hpp>

namespace youtilstest
{

namespace fs = boost::filesystem;

class PathTest
    : public TestBase
{};

TEST_F(PathTest, absolutism)
{
#define A(p)                                    \
    fs::path(p).is_absolute()

    EXPECT_TRUE(A("/"));
    EXPECT_FALSE(A("."));
    EXPECT_FALSE(A(".."));
    EXPECT_FALSE(A("./"));
    EXPECT_FALSE(A("../"));
    EXPECT_TRUE(A("/."));
    EXPECT_TRUE(A("/.."));
    EXPECT_TRUE(A("/foo/../"));
    EXPECT_TRUE(A("/foo/./"));
    EXPECT_TRUE(A("/foo/.."));

#undef A

    const fs::path empty(fs::path("/").parent_path());
    EXPECT_FALSE(empty.is_absolute());
}

TEST_F(PathTest, root_parent)
{
    const fs::path p("/");

    {
        int i = 0;
        for (auto it = p.begin(); it != p.end(); ++it)
        {
            ++i;
        }

        EXPECT_EQ(1, i);
    }

    const fs::path q(p.parent_path());

    {
        int i = 0;
        for (auto it = q.begin(); it != q.end(); ++it)
        {
            ++i;
        }

        EXPECT_EQ(0, i);
    }
}

}
