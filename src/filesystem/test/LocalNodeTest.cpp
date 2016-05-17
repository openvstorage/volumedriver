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

#include "../LocalNode.h"

namespace volumedriverfstest
{

namespace vfs = volumedriverfs;

#define LOCK_LOCKS(lnode)                            \
    std::lock_guard<decltype(vfs::LocalNode::object_lock_map_lock_)> g((lnode).object_lock_map_lock_)

class LocalNodeTest
    : public FileSystemTestBase
{
protected:
    LocalNodeTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("LocalNodeTest"))
    {}

    void
    test_lock_reaping()
    {
        const uint64_t vsize = 10ULL << 20;
        const vfs::FrontendPath fname(make_volume_name("/some-volume"));
        const auto vname(create_file(fname, vsize));

        set_lock_reaper_interval(std::numeric_limits<uint64_t>::max());

        vfs::LocalNode& lnode = *local_node(fs_->object_router());

        {
            LOCK_LOCKS(lnode);
            EXPECT_TRUE(lnode.object_lock_map_.find(vname) != lnode.object_lock_map_.end());
        }

        EXPECT_EQ(0, unlink(fname));

        {
            LOCK_LOCKS(lnode);
            EXPECT_TRUE(lnode.object_lock_map_.find(vname) != lnode.object_lock_map_.end());
        }

        set_lock_reaper_interval(1);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        {
            LOCK_LOCKS(lnode);
            EXPECT_TRUE(lnode.object_lock_map_.find(vname) == lnode.object_lock_map_.end());
        }
    }
};

TEST_F(LocalNodeTest, lock_reaping)
{
    test_lock_reaping();
}

}
