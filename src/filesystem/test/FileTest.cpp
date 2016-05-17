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

#include "FileSystemTestBase.h"

namespace volumedriverfstest
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace vfs = volumedriverfs;

class FileTest
    : public FileSystemTestBase
{
public:
    FileTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("FileTest"))
    {}
};

TEST_F(FileTest, create_and_destroy)
{
    const vfs::FrontendPath p("/some.file");

    verify_absence(p);

    create_file(p);

    EXPECT_EQ(-EEXIST,
              mknod(p,
                    S_IFREG bitor S_IWUSR bitor S_IRUSR,
                    0));

    check_stat(p, 0);

    unlink(p);

    verify_absence(p);

    EXPECT_EQ(-ENOENT, unlink(p));
}

TEST_F(FileTest, uuid_create_and_destroy)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath p("/some.file");

    verify_absence(p);

    create_file(root_id, p.str().substr(1));

    EXPECT_EQ(-EEXIST,
              mknod(root_id,
                    p.str().substr(1),
                    S_IFREG bitor S_IWUSR bitor S_IRUSR));

    const vfs::ObjectId id(*find_object(p));
    check_stat(id, 0);

    unlink(root_id, p.str().substr(1));

    verify_absence(id);

    EXPECT_EQ(-ENOENT, unlink(root_id, p.str().substr(1)));
}

TEST_F(FileTest, rename)
{
    const vfs::FrontendPath p("/some-file");

    verify_absence(p);

    create_file(p);
    check_stat(p, 0);

    const vfs::FrontendPath q("/some-file-renamed");
    verify_absence(q);

    EXPECT_EQ(0, rename(p, q));
    verify_absence(p);
    check_stat(q, 0);

    EXPECT_EQ(-ENOENT, rename(p, q));
    verify_absence(p);
    check_stat(q, 0);
}

TEST_F(FileTest, uuid_rename)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath p("/some-file");

    verify_absence(p);

    create_file(root_id, p.str().substr(1));
    const vfs::ObjectId p_id(*find_object(p));
    check_stat(p_id, 0);

    const vfs::FrontendPath q("/some-file-renamed");
    verify_absence(q);

    EXPECT_EQ(0, rename(root_id,
                        p.str().substr(1),
                        root_id,
                        q.str().substr(1)));
    verify_absence(p);

    const vfs::ObjectId q_id(*find_object(q));
    check_stat(q_id, 0);

    EXPECT_EQ(-ENOENT, rename(root_id,
                              p.str().substr(1),
                              root_id,
                              q.str().substr(1)));
    verify_absence(p);
    check_stat(q_id, 0);
}

TEST_F(FileTest, resize)
{
    const vfs::FrontendPath p("/some.file");
    create_file(p);

    check_stat(p, 0);

    const uint64_t size = 10ULL << 20;
    EXPECT_EQ(0, truncate(p, size));

    check_stat(p, size);
}

TEST_F(FileTest, uuid_resize)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath p("/some.file");
    create_file(root_id, p.str().substr(1));
    const vfs::ObjectId id(*find_object(p));

    check_stat(id, 0);

    const uint64_t size = 10ULL << 20;
    EXPECT_EQ(0, truncate(id, size));

    check_stat(id, size);
}

TEST_F(FileTest, read_write)
{
    const vfs::FrontendPath p("/some.file");
    create_file(p);

    const std::string pattern("some message");
    write_to_file(p, pattern.c_str(), pattern.size(), 0);

    check_stat(p, pattern.size());
    check_file(p, pattern, pattern.size(), 0);
}

TEST_F(FileTest, uuid_read_write)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath p("/some.file");
    create_file(root_id, p.str().substr(1));
    const vfs::ObjectId id(*find_object(p));

    const std::string pattern("some message");
    write_to_file(id, pattern.c_str(), pattern.size(), 0);

    check_stat(id, pattern.size());
    check_file(id, pattern, pattern.size(), 0);
}

TEST_F(FileTest, file_path_sanity)
{
#define OK(fp)                                  \
    EXPECT_NO_THROW(check_file_path_sanity(vfs::FrontendPath(fp)))

#define NOK(fp)                                 \
    EXPECT_THROW(check_file_path_sanity(vfs::FrontendPath(fp)), std::exception)

    NOK(".");
    NOK("..");
    NOK("/.");
    NOK("/..");
    NOK("/foo/./bar");
    NOK("/foo/../bar");
    NOK("/");
    NOK("/foo/");
    NOK("/foo/.bar/");
    NOK("foo");
    NOK("foo/");
    NOK("./foo");
    NOK("../foo");
    NOK("");

    OK("/foo");
    OK("/foo/bar");
    OK("/foo/.bar");
    OK("/.foo/.bar");
    OK("/.foo");
    OK("/..foo");

#undef OK
#undef NOK
}

TEST_F(FileTest, chmod)
{
    const vfs::FrontendPath f("/file");
    create_file(f);

    test_chmod_file(f, S_IWUSR bitor S_IRUSR bitor S_IRGRP bitor S_IROTH);
}

TEST_F(FileTest, uuid_chmod)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath f("/file");
    create_file(root_id, f.str().substr(1));
    const vfs::ObjectId id(*find_object(f));

    test_chmod_file(id, S_IWUSR bitor S_IRUSR bitor S_IRGRP bitor S_IROTH);
}

TEST_F(FileTest, chown)
{
    const vfs::FrontendPath f("/file");
    create_file(f);

    test_chown(f);
}

TEST_F(FileTest, uuid_chown)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath f("/file");
    create_file(root_id, f.str().substr(1));
    const vfs::ObjectId id(*find_object(f));

    test_chown(id);
}

TEST_F(FileTest, timestamps)
{
    const vfs::FrontendPath f("/file");
    create_file(f);
    test_timestamps(f);
}

TEST_F(FileTest, uuid_timestamps)
{
    const vfs::ObjectId root_id(*find_object(vfs::FrontendPath("/")));
    const vfs::FrontendPath f("/file");
    create_file(root_id, f.str().substr(1));
    const vfs::ObjectId id(*find_object(f));

    test_timestamps(id);
}

TEST_F(FileTest, getattr_on_inexistent_path)
{
    const vfs::FrontendPath f("/does-not-exist");
    struct stat st;
    memset(&st, 0x0, sizeof(st));
    EXPECT_THROW(fs_->getattr(f, st),
                 vfs::GetAttrOnInexistentPath);
}

// cf. OVS-2709: a backend error while removing extents of a file
// resulted in a zombie file
TEST_F(FileTest, backend_error_on_unlink)
{
    const vfs::FrontendPath f("/zombie-file");
    create_file(f);
    const std::string pattern("zombies ain't no good");
    write_to_file(f,
                  pattern.c_str(),
                  pattern.size(),
                  0);

    const vfs::ObjectId id(*find_object(f));

    be::BackendInterfacePtr bi(cm_->newBackendInterface(fdriver_namespace_));

    std::list<std::string> extents;
    bi->listObjects(extents);

    size_t removed = 0;

    for (const auto& e : extents)
    {
        if (e.substr(0, id.str().size()) == id.str())
        {
            bi->remove(e);
            ++removed;
        }
    }

    EXPECT_LT(0U,
              removed);

    EXPECT_EQ(0,
              unlink(f));

    verify_absence(f);

    std::vector<std::string> entries;
    fs_->read_dirents(vfs::FrontendPath("/"),
                      entries,
                      0);

    EXPECT_TRUE(entries.empty());

    for (const auto& e : entries)
    {
        std::cout << "unexpected entry " << e << std::endl;
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
