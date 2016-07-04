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

#include "../FileUtils.h"
#include <gtest/gtest.h>
#include <fstream>
#include <stdio.h>
#include <exception>
#include "../FileDescriptor.h"
namespace youtilstest
{
using namespace youtils;

class FileUtilsTest : public testing::Test
{};

TEST_F(FileUtilsTest, boost_rename)
{

    const fs::path t1(FileUtils::create_temp_file("/tmp", "file1"));
    ALWAYS_CLEANUP_FILE(t1);
    const fs::path t2(FileUtils::create_temp_file("/tmp", "file2"));
    ALWAYS_CLEANUP_FILE(t2);
    EXPECT_TRUE(fs::exists(t1));
    EXPECT_TRUE(fs::exists(t2));

    EXPECT_NO_THROW(fs::rename(t1, t2));
    EXPECT_FALSE(fs::exists(t1));
    EXPECT_TRUE(fs::exists(t2));
}


TEST_F(FileUtilsTest, remove)
{
    fs::path tmp = FileUtils::create_temp_file(FileUtils::temp_path(""), "help");
    ASSERT_TRUE(fs::exists(tmp));
    fs::remove(tmp);
    ASSERT_FALSE(fs::exists(tmp));
}

TEST_F(FileUtilsTest, touch)
{
    fs::path tmp = FileUtils::temp_path("help");
    FileUtils::touch(tmp);
    ASSERT_TRUE(fs::exists(tmp));
    fs::remove(tmp);
    ASSERT_FALSE(fs::exists(tmp));
}

TEST_F(FileUtilsTest, renameAndSyncFile)
{
    const fs::path tmpDir = FileUtils::temp_path("renameAndSyncFileTest");
    FileUtils::removeAllNoThrow(tmpDir);

    fs::create_directories(tmpDir);
    ALWAYS_CLEANUP_DIRECTORY(tmpDir);

    const fs::path file1 = tmpDir / "file1";
    const fs::path file2 = tmpDir / "file2";
    EXPECT_FALSE(fs::exists(file1));
    EXPECT_FALSE(fs::exists(file2));
    EXPECT_THROW(FileUtils::syncAndRename(file1, file2),
                 std::exception);
    EXPECT_FALSE(fs::exists(file1));
    EXPECT_FALSE(fs::exists(file2));

    FileUtils::touch(file1);
    EXPECT_TRUE(fs::exists(file1));
    EXPECT_FALSE(fs::exists(file2));

    EXPECT_NO_THROW(FileUtils::syncAndRename(file1, file2));
    EXPECT_FALSE(fs::exists(file1));
    EXPECT_TRUE(fs::exists(file2));

    EXPECT_THROW(FileUtils::syncAndRename(file1, file2),
                 std::exception);
    EXPECT_FALSE(fs::exists(file1));
    EXPECT_TRUE(fs::exists(file2));

    FileUtils::touch(file1);
    EXPECT_TRUE(fs::exists(file1));
    EXPECT_TRUE(fs::exists(file2));
    EXPECT_NO_THROW(FileUtils::syncAndRename(file1, file2));
    EXPECT_FALSE(fs::exists(file1));
    EXPECT_TRUE(fs::exists(file2));
    fs::remove(file2);
    fs::remove(tmpDir);
}

TEST_F(FileUtilsTest, truncate)
{
    const fs::path tmpDir = FileUtils::temp_path("someDir");
    fs::create_directories(tmpDir);
    ALWAYS_CLEANUP_DIRECTORY(tmpDir);

    EXPECT_THROW(FileUtils::truncate(tmpDir, 4096), fs::filesystem_error);

    // non-existing files neither
    const fs::path tmpFile = tmpDir / "someFile";
    EXPECT_THROW(FileUtils::truncate(tmpFile, 4096), fs::filesystem_error);

    // existing files cannot grow with truncate
    FileUtils::touch(tmpFile);
    EXPECT_THROW(FileUtils::truncate(tmpFile, 4096), fungi::IOException);

    std::ofstream f(tmpFile.string().c_str());
    const std::string s = "some nonsensical string";
    f << s;
    f.close();

    EXPECT_EQ(s.size(), fs::file_size(tmpFile));
    EXPECT_NO_THROW(FileUtils::truncate(tmpFile, s.size() - 1));
    EXPECT_EQ(s.size() - 1, fs::file_size(tmpFile));

}

TEST_F(FileUtilsTest, copy_from_nonexisting)
{
    // copy from a non-existing file
    const fs::path target = FileUtils::create_temp_file_in_temp_dir("target");
    ALWAYS_CLEANUP_FILE(target);
    const fs::path source = FileUtils::create_temp_file_in_temp_dir("source");
    ALWAYS_CLEANUP_FILE(source);
    ASSERT_TRUE(fs::exists(target));

    fs::remove(source);
    ASSERT_FALSE(fs::exists(source));

    EXPECT_THROW(FileUtils::safe_copy(source,
                                      target),
                 FileUtils::CopyNoSourceException);
}


TEST_F(FileUtilsTest, copy)
{
    std::string content("content");

    const fs::path a = FileUtils::create_temp_file_in_temp_dir();
    {
        std::ofstream ofs(a.string().c_str());
        ofs << content;
        ofs.close();
    }

    const size_t size = fs::file_size(a);


    const fs::path b = FileUtils::create_temp_file_in_temp_dir("b");
    ASSERT_NO_THROW(fs::remove(b));
    ASSERT_FALSE(fs::exists(b));

    EXPECT_NO_THROW(FileUtils::safe_copy(a, b));
    EXPECT_TRUE(fs::exists(b));
    EXPECT_EQ(size, fs::file_size(b));


    std::string read;
    {
        std::ifstream ifs(b.string().c_str());
        ifs >> read;
        ifs.close();
    }

    EXPECT_EQ(content, read);

    // copy to a larger, existing file
    read = "";
    {
        std::ofstream ofs(a.string().c_str());
        ofs << content << content << content;
        ofs.close();
    }

    EXPECT_LT(size, fs::file_size(a));

    EXPECT_NO_THROW(FileUtils::safe_copy(b, a));

    EXPECT_EQ(size, fs::file_size(a));

    {
        std::ifstream ifs(a.string().c_str());
        ifs >> read;
        ifs.close();
    }

    EXPECT_EQ(content, read);
}

}
// Local Variables: **
// End: **
