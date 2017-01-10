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
#include "../Handle.h"

#include <youtils/ScopeExit.h>
#include <youtils/System.h>

namespace volumedriverfstest
{

using namespace volumedriverfs;
namespace vd = volumedriver;
namespace yt = youtils;

namespace
{

uint64_t
erratic_get_size(vd::WeakVolumePtr v,
                 unsigned& attempt)
{
    EXPECT_FALSE(v.expired());

    if ((attempt++ % 2) == 0)
    {
        EXPECT_NO_THROW(vd::SharedVolumePtr(v));
        throw vd::TransientException("transient error");
    }
    else
    {
        return api::GetSize(v);
    }
}

}

#define LOCK_LOCKS(lnode)                                               \
    std::lock_guard<decltype(LocalNode::object_lock_map_lock_)> g((lnode).object_lock_map_lock_)

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
        const FrontendPath fname(make_volume_name("/some-volume"));
        const auto vname(create_file(fname, vsize));

        set_lock_reaper_interval(std::numeric_limits<uint64_t>::max());

        LocalNode& lnode = *local_node(fs_->object_router());

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

    // https://github.com/openvstorage/volumedriver/issues/221
    // maybe_retry_ used to std::forward its Args... - on retry, some args might
    // have been moved from already.
    // In the observed case a weak_ptr was moved from which was not reproducible
    // with g++ 4.9 (due to __weak_ptr's move ctor not resetting the internal
    // state) but only with more recent versions of g++ (5.4.0 on Ubuntu 16.04).
    void
    test_maybe_retry()
    {
        const size_t vsize = 1ULL << 20;
        const FrontendPath fname(make_volume_name("/some-volume"));
        const auto vname(create_file(fname, vsize));

        Handle::Ptr h;

        ASSERT_EQ(0, open(fname, h, O_RDONLY));
        auto on_exit(yt::make_scope_exit([&]
                                         {
                                             EXPECT_EQ(0, release(fname, std::move(h)));
                                         }));

        // populate the FastPathCookie in the Handle
        {
            std::vector<char> buf(4096);
            size_t sz = buf.size();
            bool eof = false;
            fs_->read(*h, sz, buf.data(), 0, eof);
            ASSERT_EQ(buf.size(), sz);
            ASSERT_FALSE(eof);
        }

        FastPathCookie fpc(h->cookie());
        ASSERT_TRUE(fpc != nullptr);
        unsigned attempt = 0;

        LocalNode& lnode = *local_node(fs_->object_router());
        size_t res = lnode.maybe_retry_<uint64_t,
                                        vd::WeakVolumePtr,
                                        unsigned&>(erratic_get_size,
                                                   fpc->volume,
                                                   attempt);
        EXPECT_EQ(vsize,
                  res);
    }
};

TEST_F(LocalNodeTest, lock_reaping)
{
    test_lock_reaping();
}

TEST_F(LocalNodeTest, maybe_retry)
{
    test_maybe_retry();
}

}
