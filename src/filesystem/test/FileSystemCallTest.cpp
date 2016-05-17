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
