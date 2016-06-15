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

#include "../Object.h"
#include "../ScrubTreeBuilder.h"

#include <volumedriver/Types.h>

namespace volumedriverfstest
{

using namespace std::literals::string_literals;
using namespace volumedriverfs;

namespace vd = volumedriver;

class ScrubTreeBuilderTest
    : public FileSystemTestBase
{
protected:
    ScrubTreeBuilderTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("ScrubTreeBuilderTest"))
    {}

    ObjectId
    make_clone(const ObjectId& parent,
               const std::string& name,
               const vd::SnapshotName& snap)
    {
        const FrontendPath cpath(make_volume_name(name));
        return ObjectId(fs_->create_clone(cpath,
                                          make_metadata_backend_config(),
                                          vd::VolumeId(parent.str()),
                                          snap).str());
    }

    void
    make_clones(const ObjectId& parent,
                size_t num_clones,
                size_t depth,
                ScrubManager::ClonePtrList& l)
    {
        boost::optional<vd::SnapshotName> res;

        if (depth > 0)
        {
            if (num_clones > 0)
            {
                const vd::SnapshotName snap(client_.create_snapshot(parent));
                wait_for_snapshot(parent,
                                  snap);

                for (size_t i = 0; i < num_clones; ++i)
                {
                    const std::string name("/clone-"s +
                                           boost::lexical_cast<std::string>(depth) +
                                           "-"s +
                                           boost::lexical_cast<std::string>(i) +
                                           "-"s +
                                           snap.str());

                    const ObjectId clone(make_clone(parent,
                                                    name,
                                                    snap));

                    l.emplace_back(boost::make_shared<ScrubManager::Clone>(clone));

                    make_clones(clone,
                                num_clones,
                                depth - 1,
                                l.back()->clones);
                }
            }
        }
    }

    using CloneMap = std::map<unsigned, std::set<ObjectId>>;

    CloneMap
    make_clone_map(const ScrubManager::ClonePtrList& l)
    {
        CloneMap m;
        make_clone_map(l,
                       m,
                       0);

        return m;
    }

    void
    make_clone_map(const ScrubManager::ClonePtrList& l,
                   CloneMap& m,
                   size_t idx)
    {
        for (const auto& c : l)
        {
            m[idx].emplace(c->id);
            make_clone_map(c->clones,
                           m,
                           idx + 1);
        }
    }

    std::list<vd::SnapshotName>
    list_snapshots(const ObjectId& oid)
    {
        std::list<vd::SnapshotName> snaps;

        fungi::ScopedLock l(api::getManagementMutex());
        api::showSnapshots(static_cast<const vd::VolumeId&>(oid.str()),
                           snaps);

        return snaps;
    }

    ScrubTreeBuilder
    make_scrub_tree_builder()
    {
        return ScrubTreeBuilder(fs_->object_router().object_registry()->registry());
    }
};

TEST_F(ScrubTreeBuilderTest, no_clones)
{
    const FrontendPath fname(make_volume_name("/volume"));
    const ObjectId vname(create_file(fname));
    const vd::SnapshotName snap(client_.create_snapshot(vname));
    wait_for_snapshot(vname,
                      snap);

    ScrubManager::ClonePtrList l(make_scrub_tree_builder()(vname,
                                                           snap));

    EXPECT_TRUE(l.empty());
}

TEST_F(ScrubTreeBuilderTest, children)
{
    const FrontendPath fname(make_volume_name("/volume"));
    const ObjectId vname(create_file(fname));

    size_t nchildren = 3;
    ScrubManager::ClonePtrList clones;

    make_clones(vname,
                nchildren,
                1,
                clones);

    const std::list<vd::SnapshotName> snaps(list_snapshots(vname));
    ASSERT_EQ(1,
              snaps.size());

    EXPECT_EQ(nchildren,
              clones.size());

    ScrubManager::ClonePtrList l(make_scrub_tree_builder()(vname,
                                                           snaps.back()));
    ASSERT_EQ(clones.size(),
              l.size());

    EXPECT_TRUE(make_clone_map(clones) == make_clone_map(l));
}

TEST_F(ScrubTreeBuilderTest, two_generations)
{
    const FrontendPath fname(make_volume_name("/volume"));
    const ObjectId vname(create_file(fname));

    const size_t num_clones = 2;
    const size_t depth = 2;

    ScrubManager::ClonePtrList clones;
    make_clones(vname,
                num_clones,
                depth,
                clones);

    EXPECT_EQ(num_clones,
              clones.size());

    const CloneMap m1(make_clone_map(clones));

    ASSERT_FALSE(m1.empty());

    const std::list<vd::SnapshotName> snaps(list_snapshots(vname));
    ASSERT_EQ(1,
              snaps.size());

    const CloneMap
        m2(make_clone_map(make_scrub_tree_builder()(vname,
                                                    snaps.back())));

    ASSERT_EQ(m1.size(),
              m2.size());

    if (m1 != m2)
    {
        auto fun([](const CloneMap& m)
                 {
                     for (const auto& p : m)
                     {
                         std::cout << "key: " << p.first << std::endl;
                         std::cout << "set: {";
                         for (const auto& o : p.second)
                         {
                             std::cout << o << ", ";
                         }
                         std::cout << "}" << std::endl;
                     }
                 });

        fun(m1);
        fun(m2);
    }

    EXPECT_TRUE(m1 == m2);
}

TEST_F(ScrubTreeBuilderTest, two_snapshots)
{
    const FrontendPath fname(make_volume_name("/volume"));
    const ObjectId vname(create_file(fname));

    ScrubManager::ClonePtrList clones1;
    make_clones(vname,
                1,
                1,
                clones1);

    EXPECT_EQ(1,
              clones1.size());

    ScrubManager::ClonePtrList clones2;
    make_clones(vname,
                1,
                1,
                clones2);

    EXPECT_EQ(1,
              clones2.size());

    const std::list<vd::SnapshotName> snaps(list_snapshots(vname));
    ASSERT_EQ(2,
              snaps.size());

    const CloneMap map1(make_clone_map(clones1));
    const CloneMap map2(make_clone_map(clones2));

    {
        const CloneMap m(make_clone_map(make_scrub_tree_builder()(vname,
                                                                  snaps.back())));
        EXPECT_FALSE(m.empty());
        EXPECT_EQ(map2.size(),
                  m.size());
        EXPECT_TRUE(m == map2);
    }

    {
        const CloneMap m(make_clone_map(make_scrub_tree_builder()(vname,
                                                                  snaps.front())));
        EXPECT_FALSE(m.empty());

        CloneMap u;
        for (const auto& o : map1)
        {
            u[o.first].insert(o.second.begin(),
                              o.second.end());
        }

        for (const auto& o : map2)
        {
            u[o.first].insert(o.second.begin(),
                              o.second.end());
        }

        EXPECT_TRUE(m == u);
    }
}

}
