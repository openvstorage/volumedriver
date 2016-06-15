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
