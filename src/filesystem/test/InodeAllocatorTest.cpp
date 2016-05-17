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

#include "RegistryTestSetup.h"

#include <set>

#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>

#include "../DirectoryEntry.h"
#include "../InodeAllocator.h"

namespace volumedriverfstest
{

namespace ara = arakoon;
namespace vfs = volumedriverfs;

class InodeAllocatorTest
    : public RegistryTestSetup
{
protected:
    InodeAllocatorTest()
        : RegistryTestSetup("InodeAllocatorTest")
    {}
};

TEST_F(InodeAllocatorTest, construction)
{
    const vfs::ClusterId cluster_id("cluster");

    ASSERT_EQ(0U, registry_->prefix(std::string("")).size());

    vfs::InodeAllocator alloc(cluster_id,
                              registry_);

    const ara::value_list l(registry_->prefix(std::string("")));
    EXPECT_EQ(1U, l.size());

    auto it = l.begin();
    ara::arakoon_buffer kbuf;

     ASSERT_TRUE(it.next(kbuf));

    {
        const std::string k(static_cast<const char*>(kbuf.second),
                            kbuf.first);
        EXPECT_EQ(alloc.make_key(), k);
    }

    EXPECT_FALSE(it.next(kbuf));
}

TEST_F(InodeAllocatorTest, alloc)
{
    const vfs::ClusterId cluster_id("cluster");
    vfs::InodeAllocator alloc(cluster_id,
                              registry_);

    for (unsigned i = 1; i < 43; ++i)
    {
        vfs::Inode inode(alloc());
        EXPECT_TRUE(vfs::Inode(i) == inode);
    }
}

TEST_F(InodeAllocatorTest, restart)
{
    const vfs::ClusterId cluster_id("cluster");
    const unsigned count = 7;
    vfs::Inode last(0);

    {
        vfs::InodeAllocator alloc(cluster_id,
                                  registry_);

        for (unsigned i = 1; i < count; ++i)
        {
            last = alloc();
        }
    }

    vfs::InodeAllocator alloc(cluster_id,
                              registry_);

    EXPECT_TRUE(vfs::Inode(last + 1) == alloc());
}

TEST_F(InodeAllocatorTest, cleanup)
{
    const vfs::ClusterId cluster_id("cluster");
    const vfs::Inode first(1);

    {
        vfs::InodeAllocator alloc(cluster_id,
                                  registry_);

        EXPECT_TRUE(first == alloc());

        for (unsigned i = 0; i < 10; ++i)
        {
            EXPECT_TRUE(first != alloc());
        }

        EXPECT_TRUE(registry_->exists(alloc.make_key()));

        alloc.destroy();
        EXPECT_FALSE(registry_->exists(alloc.make_key()));

    }

    vfs::InodeAllocator alloc(cluster_id,
                              registry_);

    EXPECT_TRUE(first == alloc());
}

TEST_F(InodeAllocatorTest, stress)
{
    boost::mutex lock;
    boost::condition_variable cond;
    bool run = false;

    std::set<vfs::Inode> inodes;

    const unsigned nworkers = 4;

    std::vector<boost::thread> workers;
    workers.reserve(nworkers);

    const unsigned allocs_per_worker = 13;
    const vfs::ClusterId cluster_id("cluster");

    {
        boost::lock_guard<decltype(lock)> g(lock);

        for (unsigned i = 0; i < nworkers; ++i)
        {
            auto fun([&]
                     {
                         vfs::InodeAllocator alloc(cluster_id,
                                                   registry_);

                         {
                             boost::unique_lock<decltype(lock)> u(lock);
                             cond.wait(u, [&]{ return (run == true); });

                         }

                         for (unsigned j = 0; j < allocs_per_worker; ++j)
                         {
                             const vfs::Inode inode(alloc());
                             boost::lock_guard<decltype(lock)> g(lock);
                             inodes.insert(inode);
                         }
                     });

            workers.emplace_back(boost::thread(fun));
        }

        run = true;
        cond.notify_all();
    }

    for (auto& w : workers)
    {
        w.join();
    }

    for (unsigned i = 1; i <= nworkers * allocs_per_worker; ++i)
    {
        auto it = inodes.find(vfs::Inode(i));
        EXPECT_TRUE(it != inodes.end());
        inodes.erase(it);
    }

    EXPECT_TRUE(inodes.empty());
}

}
