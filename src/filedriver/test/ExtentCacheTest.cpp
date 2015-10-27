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

#include <boost/filesystem.hpp>

#include <youtils/FileUtils.h>
#include <youtils/TestBase.h>

#include <filedriver/ExtentCache.h>

namespace filedrivertest
{
namespace fd = filedriver;
namespace fs = boost::filesystem;
namespace yt = youtils;

class ExtentCacheTest
    : public youtilstest::TestBase
{
protected:
    ExtentCacheTest()
        : youtilstest::TestBase()
        , path_(yt::FileUtils::temp_path () / "ExtentCacheTest")
    {}

    virtual void
    SetUp()
    {
        fs::remove_all(path_);
        fs::create_directories(path_);
    }

    virtual void
    TearDown()
    {
        fs::remove_all(path_);
    }

    const fs::path path_;
};

TEST_F(ExtentCacheTest, too_small)
{
    EXPECT_THROW(fd::ExtentCache(path_, 0),
                 std::exception);
}

TEST_F(ExtentCacheTest, cache_path_is_not_a_directory)
{
    const fs::path p(path_ / "file");
    {
        fs::ofstream ofs(p);
        ofs << "this ain't no directory" << std::endl;
    }


    EXPECT_THROW(fd::ExtentCache(p, 10),
                 std::exception);
}

TEST_F(ExtentCacheTest, resize)
{
    size_t cap = 1;
    fd::ExtentCache cache(path_, cap);

    EXPECT_EQ(cap, cache.capacity());

    EXPECT_THROW(cache.capacity(0),
                 std::exception);

    EXPECT_EQ(cap, cache.capacity());

    cap = 10;
    cache.capacity(cap);
    EXPECT_EQ(cap, cache.capacity());

    cap = 1;
    cache.capacity(cap);
    EXPECT_EQ(cap, cache.capacity());
}

}
