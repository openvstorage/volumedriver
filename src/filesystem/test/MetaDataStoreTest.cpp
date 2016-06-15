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

#include <youtils/LockedArakoon.h>
#include <filesystem/MetaDataStore.h>

#include <youtils/UUID.h>

#include <boost/smart_ptr/make_shared.hpp>

namespace volumedriverfstest
{

namespace vfs = volumedriverfs;
namespace yt = youtils;

TODO("AR: use ::testing::WithParamInterface<vfs::UseCache>");
// ... that requires however a newer gtest.

class MetaDataStoreTest
    : public RegistryTestSetup
{
protected:
    MetaDataStoreTest()
        : RegistryTestSetup("MetaDataStoreTest")
        , cluster_id_("cluster")
    {}

    const vfs::ClusterId cluster_id_;
};

TEST_F(MetaDataStoreTest, root)
{
    vfs::MetaDataStore mds(std::static_pointer_cast<yt::LockedArakoon>(registry_),
                           cluster_id_,
                           vfs::UseCache::F);

    const vfs::FrontendPath root("/");
    vfs::DirectoryEntryPtr dentry(mds.find(root));

    ASSERT_TRUE(dentry != nullptr);
    EXPECT_EQ(root,
              mds.find_path(dentry->object_id()));

    vfs::DirectoryEntryPtr dentry2(mds.find(dentry->object_id()));
    ASSERT_TRUE(dentry2 != nullptr);
}

TEST_F(MetaDataStoreTest, path_and_id)
{
    vfs::MetaDataStore mds(std::static_pointer_cast<yt::LockedArakoon>(registry_),
                           cluster_id_,
                           vfs::UseCache::T);

    const vfs::FrontendPath dpath("/dir");
    const vfs::Permissions pms(S_IWUSR bitor S_IRUSR);

    vfs::DirectoryEntryPtr
        dentry(boost::make_shared<vfs::DirectoryEntry>(vfs::DirectoryEntry::Type::Directory,
                                                       mds.alloc_inode(),
                                                       pms,
                                                       vfs::UserId(::getuid()),
                                                       vfs::GroupId(::getgid())));

    mds.add(dpath,
            dentry);

    ASSERT_EQ(dpath,
              mds.find_path(dentry->object_id()));

    {
        const vfs::DirectoryEntryPtr p(mds.find(dentry->object_id()));
        ASSERT_TRUE(p != nullptr);
        ASSERT_TRUE(*dentry == *p);
    }

    const vfs::FrontendPath fpath(dpath / "file");
    vfs::DirectoryEntryPtr
        fentry(boost::make_shared<vfs::DirectoryEntry>(vfs::DirectoryEntry::Type::File,
                                                       mds.alloc_inode(),
                                                       pms,
                                                       vfs::UserId(::getuid()),
                                                       vfs::GroupId(::getgid())));

    mds.add(dentry->object_id(),
            fpath.filename().string(),
            fentry);

    ASSERT_EQ(fpath,
              mds.find_path(fentry->object_id()));

    {
        const vfs::DirectoryEntryPtr p(mds.find(fentry->object_id()));
        ASSERT_TRUE(p != nullptr);
        ASSERT_TRUE(*fentry == *p);
    }
}

// cf. OVS-1381:
// conflicting updates of volume entries could lead to an infinite loop around
// test_and_set as upon conflict detection the cached entry wasn't dropped and hence
// used in all subsequent iterations.
TEST_F(MetaDataStoreTest, conflicting_updates_for_volumes)
{
    vfs::MetaDataStore mds1(std::static_pointer_cast<yt::LockedArakoon>(registry_),
                            cluster_id_,
                            vfs::UseCache::T);

    const vfs::FrontendPath path("/volume");
    const vfs::Permissions perms(S_IWUSR bitor S_IRUSR);

    {
        vfs::DirectoryEntryPtr
            dentry(boost::make_shared<vfs::DirectoryEntry>(vfs::DirectoryEntry::Type::Volume,
                                                           mds1.alloc_inode(),
                                                           perms,
                                                           vfs::UserId(::getuid()),
                                                           vfs::GroupId(::getgid())));

        mds1.add(path, dentry);
    }

    {
        vfs::DirectoryEntryPtr dentry(mds1.find(path));
        ASSERT_TRUE(dentry != nullptr);
        ASSERT_EQ(perms, dentry->permissions());
    }

    vfs::MetaDataStore mds2(std::static_pointer_cast<yt::LockedArakoon>(registry_),
                            cluster_id_,
                            vfs::UseCache::T);

    const vfs::Permissions perms2(S_IWUSR);
    mds2.chmod(path, perms2);

    {
        vfs::DirectoryEntryPtr dentry(mds2.find(path));
        ASSERT_TRUE(dentry != nullptr);
        ASSERT_EQ(perms2, dentry->permissions());
    }

    // on mds1 the entry is cached and the change is not visible!
    {
        vfs::DirectoryEntryPtr dentry(mds1.find(path));
        ASSERT_TRUE(dentry != nullptr);
        ASSERT_EQ(perms, dentry->permissions());
    }

    const vfs::Permissions perms3(S_IRUSR);

    // this used to end up in an infinite loop around test_and_set as the cached
    // value was used in it.
    mds1.chmod(path, perms3);

    {
        vfs::DirectoryEntryPtr dentry(mds1.find(path));
        ASSERT_TRUE(dentry != nullptr);
        ASSERT_EQ(perms3, dentry->permissions());
    }
}

TEST_F(MetaDataStoreTest, uuid_conflicting_updates_for_volumes)
{
    vfs::MetaDataStore mds1(std::static_pointer_cast<yt::LockedArakoon>(registry_),
                            cluster_id_,
                            vfs::UseCache::T);

    const vfs::DirectoryEntryPtr root_dentry(mds1.find(vfs::FrontendPath("/")));
    const vfs::ObjectId parent_id(root_dentry->object_id());

    const vfs::Permissions perms(S_IWUSR bitor S_IRUSR);
    const std::string root("volume");

    vfs::DirectoryEntryPtr
        dentry(boost::make_shared<vfs::DirectoryEntry>(vfs::DirectoryEntry::Type::Volume,
                                                       mds1.alloc_inode(),
                                                       perms,
                                                       vfs::UserId(::getuid()),
                                                       vfs::GroupId(::getgid())));
    mds1.add(parent_id, root, dentry);

    {
        vfs::DirectoryEntryPtr volume_dentry(mds1.find(dentry->object_id()));
        ASSERT_TRUE(volume_dentry != nullptr);
        ASSERT_EQ(perms, volume_dentry->permissions());
    }

    vfs::MetaDataStore mds2(std::static_pointer_cast<yt::LockedArakoon>(registry_),
                            cluster_id_,
                            vfs::UseCache::T);

    const vfs::Permissions perms2(S_IWUSR);
    mds2.chmod(dentry->object_id(), perms2);

    {
        vfs::DirectoryEntryPtr volume_dentry(mds2.find(dentry->object_id()));
        ASSERT_TRUE(volume_dentry != nullptr);
        ASSERT_EQ(perms2, volume_dentry->permissions());
    }

    {
        vfs::DirectoryEntryPtr volume_dentry(mds1.find(dentry->object_id()));
        ASSERT_TRUE(volume_dentry != nullptr);
        ASSERT_EQ(perms, volume_dentry->permissions());
    }

    const vfs::Permissions perms3(S_IRUSR);

    mds1.chmod(dentry->object_id(), perms3);

    {
        vfs::DirectoryEntryPtr volume_dentry(mds1.find(dentry->object_id()));
        ASSERT_TRUE(dentry != nullptr);
        ASSERT_EQ(perms3, volume_dentry->permissions());
    }
}

}
