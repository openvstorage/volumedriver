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
