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
