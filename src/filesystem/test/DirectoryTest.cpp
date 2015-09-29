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

#include "FileSystemTestBase.h"

namespace volumedriverfstest
{

namespace fs = boost::filesystem;
namespace vfs = volumedriverfs;

class DirectoryTest
    : public FileSystemTestBase
{
protected:
    DirectoryTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("DirectoryTest"))
    {}
};

TEST_F(DirectoryTest, open_close)
{
    const vfs::FrontendPath d("/directory");

    verify_absence(d);

    vfs::Handle::Ptr h;

    EXPECT_EQ(-ENOENT, opendir(d, h));
    // closedir(3) says EBADF is to be expected, but we're actually getting EINVAL back
    // from the internal closedir(0);
    EXPECT_GT(0, releasedir(d, std::move(h)));

    create_directory(d);
    EXPECT_EQ(0, opendir(d, h));
    EXPECT_EQ(0, releasedir(d, std::move(h)));
    EXPECT_GT(0, releasedir(d, std::move(h)));
}

TEST_F(DirectoryTest, create_and_destroy)
{
    const vfs::FrontendPath p("/some.directory");
    verify_absence(p);

    create_directory(p);
    check_dir_stat(p);

    EXPECT_EQ(-EEXIST, mkdir(p,
                             S_IFDIR bitor S_IRWXU));
    EXPECT_EQ(0, rmdir(p));
    verify_absence(p);
}

TEST_F(DirectoryTest, uuid_create_and_destroy)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath p("/some.directory");
    verify_absence(p);

    create_directory(root_id, p.str().substr(1));
    const vfs::ObjectId id(*find_object(p));
    check_dir_stat(id);

    EXPECT_EQ(-EEXIST, mkdir(root_id,
                             p.str().substr(1),
                             S_IFDIR bitor S_IRWXU));
    EXPECT_EQ(0, rmdir(id));
    verify_absence(id);
}

TEST_F(DirectoryTest, remove_non_empty)
{
    const vfs::FrontendPath d("/some.directory");
    create_directory(d);
    check_dir_stat(d);

    const fs::path tmp(fs::path(d.str()) / "some.file");
    const vfs::FrontendPath f(tmp.string());
    create_file(f);

    EXPECT_EQ(-ENOTEMPTY, rmdir(d));
    check_dir_stat(d);

    unlink(f);

    EXPECT_EQ(0, rmdir(d));

    struct stat st;
    EXPECT_EQ(-ENOENT, getattr(d, st));
}

TEST_F(DirectoryTest, uuid_remove_non_empty)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath d("/some.directory");

    create_directory(root_id, d.str().substr(1));
    const vfs::ObjectId id(*find_object(d));
    check_dir_stat(id);

    const std::string f("some.file");
    create_file(id, f);

    EXPECT_EQ(-ENOTEMPTY, rmdir(id));
    check_dir_stat(id);

    unlink(id, f);

    EXPECT_EQ(0, rmdir(id));

    struct stat st;
    EXPECT_EQ(-ENOENT, getattr(id, st));
}

TEST_F(DirectoryTest, rmdir_file)
{
    const vfs::FrontendPath f("/file");
    verify_absence(f);

    create_file(f);
    check_stat(f, 0);

    EXPECT_EQ(-ENOTDIR, rmdir(f));
}

TEST_F(DirectoryTest, uuid_rmdir_file)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath f("/file");
    verify_absence(f);

    create_file(root_id, f.str().substr(1));
    const vfs::ObjectId id(*find_object(f));
    check_stat(id, 0);

    EXPECT_EQ(-ENOTDIR, rmdir(id));
}

TEST_F(DirectoryTest, rmdir_volume)
{
    const vfs::FrontendPath v(make_volume_name("/volume"));
    verify_absence(v);

    create_file(v);
    check_stat(v, 0);

    EXPECT_EQ(-ENOTDIR, rmdir(v));
}

TEST_F(DirectoryTest, uuid_rmdir_volume)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath v(make_volume_name("/volume"));
    verify_absence(v);

    create_file(root_id,
                v.str().substr(1));
    const vfs::ObjectId id(*find_object(v));
    check_stat(id, 0);

    EXPECT_EQ(-ENOTDIR, rmdir(id));
}

TEST_F(DirectoryTest, chmod)
{
    const vfs::FrontendPath d("/directory");
    create_directory(d);

    const mode_t mode = S_IRWXU;
    EXPECT_EQ(0, chmod(d, mode));

    check_dir_stat(d, mode);
}

TEST_F(DirectoryTest, uuid_chmod)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath d("/directory");
    create_directory(root_id, d.str().substr(1));
    const vfs::ObjectId id(*find_object(d));

    const mode_t mode = S_IRWXU;
    EXPECT_EQ(0, chmod(id, mode));

    check_dir_stat(id, mode);
}

TEST_F(DirectoryTest, chown)
{
    const vfs::FrontendPath d("/directory");
    create_directory(d);

    test_chown(d);
}

TEST_F(DirectoryTest, uuid_chown)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath d("/directory");
    create_directory(root_id, d.str().substr(1));
    const vfs::ObjectId id(*find_object(d));

    test_chown(id);
}

TEST_F(DirectoryTest, timestamps)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath d("/directory");
    create_directory(root_id, d.str().substr(1));
    const vfs::ObjectId id(*find_object(d));

    test_timestamps(id);
}

}
