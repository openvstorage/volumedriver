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

#include <volumedriver/Api.h>

namespace volumedriverfstest
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace vfs = volumedriverfs;

class RestartTest
    : public FileSystemTestBase
{
public:
    RestartTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("RestartTest"))
    {}
};

#define LOCKVD() \
    fungi::ScopedLock ag__(api::getManagementMutex())

TEST_F(RestartTest, happy_path)
{
    const size_t vsize = 1 << 20;

    const vfs::FrontendPath fname1(make_volume_name("/volume1"));
    const vd::VolumeId vname1(create_file(fname1, vsize));
    const size_t csize = get_cluster_size(vfs::ObjectId(vname1.str()));

    const vfs::FrontendPath fname2(make_volume_name("/volume2"));
    const vd::VolumeId vname2(create_file(fname2, vsize));

    const vfs::FrontendPath fname3("/some.file.that.is.not.a.volume");
    create_file(fname3, vsize);

    const std::string pattern1("Herzog");
    const size_t wsize = 2 * csize;
    const off_t off = csize - 1;
    write_to_file(fname1, pattern1, wsize, off);

    const std::string pattern2("Springtime Epigram");
    write_to_file(fname2, pattern2, wsize, off);

    const std::string pattern3("Ted");
    write_to_file(fname3, pattern3, wsize, off);

    {
        LOCKVD();
        EXPECT_NO_THROW(api::getVolumePointer(vname1));
        EXPECT_NO_THROW(api::getVolumePointer(vname2));
    }

    stop_fs();
    start_fs();

    {
        LOCKVD();
        EXPECT_NO_THROW(api::getVolumePointer(vname1));
        EXPECT_NO_THROW(api::getVolumePointer(vname2));
    }

    check_file(fname1, pattern1, wsize, off);
    check_file(fname2, pattern2, wsize, off);
    check_file(fname3, pattern3, wsize, off);
}

TEST_F(RestartTest, offlined_node)
{
    std::shared_ptr<vfs::ClusterRegistry> reg(cluster_registry(fs_->object_router()));

    stop_fs();

    reg->set_node_state(local_node_id(),
                        vfs::ClusterNodeStatus::State::Offline);

    EXPECT_EQ(vfs::ClusterNodeStatus::State::Offline,
              reg->get_node_state(local_node_id()));

    ASSERT_NO_THROW(start_fs());

    EXPECT_EQ(vfs::ClusterNodeStatus::State::Online,
              reg->get_node_state(local_node_id()));
}

TEST_F(RestartTest, owner_tag)
{
    const vfs::FrontendPath fname(make_volume_name("/volume"));
    const vfs::ObjectId oid(create_file(fname));
    const vfs::ObjectRegistrationPtr reg(find_registration(oid));

    {
        LOCKVD();

        const vd::VolumeConfig
            cfg(api::getVolumeConfig(static_cast<vd::VolumeId>(oid)));
        EXPECT_EQ(reg->owner_tag,
                  cfg.owner_tag_);
    }

    stop_fs();
    start_fs();

    LOCKVD();

    const vd::VolumeConfig
        cfg(api::getVolumeConfig(static_cast<vd::VolumeId>(oid)));
    EXPECT_EQ(reg->owner_tag,
              cfg.owner_tag_);
}

}

// Local Variables: **
// mode: c++ **
// End: **
