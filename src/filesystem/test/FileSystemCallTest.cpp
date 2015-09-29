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

#include "../FileSystemCall.h"

#include <youtils/TestBase.h>

namespace volumedriverfstest
{

namespace vfs = volumedriverfs;
namespace ytt = youtilstest;

class FileSystemCallTest
    : public ytt::TestBase
{
public:
    static void
    test_roundtrip(vfs::FileSystemCall c)
    {
        std::stringstream ss;
        ss << c;
        ASSERT_TRUE(ss.good());
        ASSERT_FALSE(ss.bad());
        ASSERT_FALSE(ss.fail());

        vfs::MaybeFileSystemCall m;
        ss >> m;

        ASSERT_FALSE(ss.bad());
        ASSERT_FALSE(ss.fail());

        ASSERT_TRUE(static_cast<bool>(m));
        EXPECT_EQ(*m, c);
    }
};

TEST_F(FileSystemCallTest, roundtrip)
{
#define T(c)                                    \
    test_roundtrip(vfs::FileSystemCall::c)

    T(Getattr);
    T(Access);
    T(Readlink);
    T(Readdir);
    T(Mknod);
    T(Mkdir);
    T(Unlink);
    T(Rmdir);
    T(Symlink);
    T(Rename);
    T(Link);
    T(Chown);
    T(Chmod);
    T(Truncate);
    T(Open);
    T(Read);
    T(Write);
    T(Statfs);
    T(Utimens);
    T(Release);
    T(Fsync);
    T(Opendir);
    T(Releasedir);

#undef T
}

TEST_F(FileSystemCallTest, stream_in_unknown_call)
{
    vfs::MaybeFileSystemCall m;
    std::stringstream ss;
    ss << "fubar";

    ss >> m;

    EXPECT_FALSE(m);
    EXPECT_TRUE(ss.fail());
}

}
