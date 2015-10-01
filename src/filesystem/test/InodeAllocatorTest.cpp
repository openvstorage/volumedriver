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
