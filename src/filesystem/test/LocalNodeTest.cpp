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
