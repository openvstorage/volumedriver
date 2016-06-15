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

#include <filesystem/FileSystem.h>

namespace volumedriverfstest
{

namespace vfs = volumedriverfs;

class FileSystemTest
    : public FileSystemTestBase
{
public:
    FileSystemTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("FileSystemTest"))
    {}
};

TEST_F(FileSystemTest, inode_allocation)
{
    const vfs::FrontendPath p("/foo");
    const vfs::FrontendPath q("/fooo");
    const vfs::FrontendPath v(make_volume_name("/goo"));
    const vfs::FrontendPath w(make_volume_name("/gooo"));

    EXPECT_EQ(vfs::FileSystem::inode(p), vfs::FileSystem::inode(p));
    EXPECT_EQ(vfs::FileSystem::inode(q), vfs::FileSystem::inode(q));
    EXPECT_EQ(vfs::FileSystem::inode(v), vfs::FileSystem::inode(v));
    EXPECT_EQ(vfs::FileSystem::inode(w), vfs::FileSystem::inode(w));

    EXPECT_NE(vfs::FileSystem::inode(p), vfs::FileSystem::inode(q));
    EXPECT_NE(vfs::FileSystem::inode(p), vfs::FileSystem::inode(v));
    EXPECT_NE(vfs::FileSystem::inode(p), vfs::FileSystem::inode(w));
}

}
