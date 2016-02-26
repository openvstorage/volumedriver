// Copyright 2015 iNuron NV
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
        EXPECT_TRUE(api::getVolumePointer(vname1) != nullptr);
        EXPECT_TRUE(api::getVolumePointer(vname2) != nullptr);
    }

    stop_fs();
    start_fs();

    {
        LOCKVD();
        EXPECT_TRUE(api::getVolumePointer(vname1) != nullptr);
        EXPECT_TRUE(api::getVolumePointer(vname2) != nullptr);
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
