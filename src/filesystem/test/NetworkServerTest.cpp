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

#include "../PythonClient.h"
#include "../NetworkXioInterface.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <youtils/ArakoonInterface.h>
#include <youtils/Catchers.h>
#include <youtils/FileUtils.h>
#include <youtils/FileDescriptor.h>
#include <youtils/wall_timer.h>

#include <volumedriver/Api.h>
#include <volumedriver/Volume.h>

#include <filesystem/ObjectRouter.h>
#include <filesystem/Registry.h>
#include <filesystem/c-api/volumedriver.h>

namespace volumedriverfstest
{

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace vfs = volumedriverfs;

using namespace volumedriverfs;

class NetworkServerTest
    : public FileSystemTestBase
{
public:
    NetworkServerTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("NetworkServerTest")
                             .redirect_timeout_ms(10000)
                             .backend_sync_timeout_ms(9500)
                             .migrate_timeout_ms(500)
                             .redirect_retries(1))
    {}

    virtual void
    SetUp()
    {
        FileSystemTestBase::SetUp();
        bpt::ptree pt;

        net_xio_server_ = std::make_unique<NetworkXioInterface>(pt,
                                                                RegisterComponent::F,
                                                                *fs_);

        net_xio_thread_ = boost::thread([&]{
                    ASSERT_NO_THROW(net_xio_server_->run());
                });
    }

    virtual void
    TearDown()
    {
        net_xio_server_->shutdown();
        net_xio_thread_.join();
        net_xio_server_ = nullptr;
        FileSystemTestBase::TearDown();
    }

    std::unique_ptr<NetworkXioInterface> net_xio_server_;
    boost::thread net_xio_thread_;
};

TEST_F(NetworkServerTest, create_destroy_context)
{
    //uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "tcp",
                                         "127.0.0.1",
                                         21321));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);

    const vfs::FrontendPath fname(make_volume_name("/volume"));

    verify_absence(fname);

    const vfs::ObjectId vname(create_file(fname));
    verify_registration(vname, local_node_id());

    EXPECT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

} //namespace
