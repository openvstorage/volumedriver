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
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "tcp",
                                         "127.0.0.1",
                                         21321));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    EXPECT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, non_existent_volume)
{
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "tcp",
                                         "127.0.0.1",
                                         21321));

    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(-1,
              ovs_ctx_init(ctx, "volume", O_RDWR));
    EXPECT_EQ(EACCES, errno);
    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, remove_non_existent_volume)
{
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "tcp",
                                         "127.0.0.1",
                                         21321));

    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(-1,
              ovs_remove_volume(ctx, "volume"));
    EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, already_created_volume)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "tcp",
                                         "127.0.0.1",
                                         21321));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    EXPECT_EQ(-1,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_EQ(0,
              ovs_remove_volume(ctx, "volume"));
    EXPECT_EQ(-1,
              ovs_remove_volume(ctx, "volume"));
    EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, open_same_volume_twice)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "tcp",
                                         "127.0.0.1",
                                         21321));
    ovs_ctx_t *ctx1 = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx1 != nullptr);
    ovs_ctx_t *ctx2 = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx2 != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx1,
                                "volume",
                                volume_size));
    EXPECT_EQ(0,
              ovs_ctx_init(ctx1,
                           "volume",
                           O_RDWR));
    EXPECT_EQ(0,
              ovs_ctx_init(ctx2,
                           "volume",
                           O_RDWR));
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx1));
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx2));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, create_write_read_destroy)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "tcp",
                                         "127.0.0.1",
                                         21321));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    EXPECT_EQ(0,ovs_ctx_init(ctx,
                             "volume",
                             O_RDWR));

    std::string pattern("openvstorage1");
    auto wbuf = std::make_unique<uint8_t[]>(pattern.length());
    ASSERT_TRUE(wbuf != nullptr);

    memcpy(wbuf.get(),
           pattern.c_str(),
           pattern.length());

    struct ovs_aiocb w_aio;
    w_aio.aio_nbytes = pattern.length();
    w_aio.aio_offset = 0;
    w_aio.aio_buf = wbuf.get();

    EXPECT_EQ(0,
              ovs_aio_write(ctx,
                            &w_aio));

    EXPECT_EQ(0,
              ovs_aio_suspend(ctx,
                              &w_aio,
                              NULL));

    EXPECT_EQ(pattern.length(),
              ovs_aio_return(ctx,
                             &w_aio));

    EXPECT_EQ(0,
              ovs_aio_finish(ctx, &w_aio));

    wbuf.reset();

    auto rbuf = std::make_unique<uint8_t[]>(pattern.length());
    ASSERT_TRUE(rbuf != nullptr);

    struct ovs_aiocb r_aio;
    r_aio.aio_nbytes = pattern.length();
    r_aio.aio_offset = 0;
    r_aio.aio_buf = rbuf.get();

    EXPECT_EQ(0,
              ovs_aio_read(ctx,
                           &r_aio));

    EXPECT_EQ(0,
              ovs_aio_suspend(ctx,
                              &r_aio,
                              NULL));

    EXPECT_EQ(pattern.length(),
              ovs_aio_return(ctx,
                             &r_aio));

    EXPECT_EQ(0,
              ovs_aio_finish(ctx, &r_aio));

    EXPECT_TRUE(memcmp(rbuf.get(),
                       pattern.c_str(),
                       pattern.length()) == 0);
    rbuf.reset();

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, completion)
{
    struct completion_function
    {
        static void finish_write(ovs_completion_t *comp, void *arg)
        {
            ssize_t *len = (ssize_t*)arg;
            EXPECT_EQ(*len,
                      ovs_aio_return_completion(comp));
            EXPECT_EQ(0,
                      ovs_aio_signal_completion(comp));
        }
        static void finish_read(ovs_completion_t *comp, void *arg)
        {
            ssize_t *len = (ssize_t*)arg;
            EXPECT_EQ(*len,
                      ovs_aio_return_completion(comp));
            EXPECT_EQ(0,
                      ovs_aio_signal_completion(comp));
        }
        static void finish_flush(ovs_completion_t *comp, void * /*arg*/)
        {
            EXPECT_EQ(0,
                      ovs_aio_return_completion(comp));
            EXPECT_EQ(0,
                      ovs_aio_signal_completion(comp));
        }
    };

    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "tcp",
                                         "127.0.0.1",
                                         21321));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    EXPECT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));

    ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

    std::string pattern("openvstorage1");
    ssize_t pattern_len = pattern.length();
    auto wbuf = std::make_unique<uint8_t[]>(pattern_len);
    ASSERT_TRUE(wbuf != nullptr);

    ovs_completion_t *w_completion =
        ovs_aio_create_completion(completion_function::finish_write,
                                  &pattern_len);

    ovs_completion_t *f_completion =
        ovs_aio_create_completion(completion_function::finish_flush,
                                  NULL);

    memcpy(wbuf.get(),
           pattern.c_str(),
           pattern_len);

    struct ovs_aiocb w_aio;
    w_aio.aio_nbytes = pattern_len;
    w_aio.aio_offset = 0;
    w_aio.aio_buf = wbuf.get();

    EXPECT_EQ(0,
              ovs_aio_writecb(ctx,
                              &w_aio,
                              w_completion));

    EXPECT_EQ(0,
              ovs_aio_flushcb(ctx,
                              f_completion));

    EXPECT_EQ(0,
              ovs_aio_suspend(ctx,
                              &w_aio,
                              NULL));

    EXPECT_EQ(pattern_len,
              ovs_aio_return(ctx,
                             &w_aio));

    EXPECT_EQ(0,
              ovs_aio_wait_completion(w_completion, NULL));
    EXPECT_EQ(0,
              ovs_aio_release_completion(w_completion));

    EXPECT_EQ(0,
              ovs_aio_wait_completion(f_completion, NULL));
    EXPECT_EQ(0,
              ovs_aio_release_completion(f_completion));

    EXPECT_EQ(0,
              ovs_aio_finish(ctx, &w_aio));

    wbuf.reset();

    auto rbuf = std::make_unique<uint8_t[]>(pattern_len);
    ASSERT_TRUE(rbuf != nullptr);

    struct ovs_aiocb r_aio;
    r_aio.aio_nbytes = pattern_len;
    r_aio.aio_offset = 0;
    r_aio.aio_buf = rbuf.get();

    ovs_completion_t *r_completion =
        ovs_aio_create_completion(completion_function::finish_read,
                                  &pattern_len);

    EXPECT_EQ(0,
              ovs_aio_readcb(ctx,
                             &r_aio,
                             r_completion));

    EXPECT_EQ(0,
              ovs_aio_suspend(ctx,
                              &r_aio,
                              NULL));

    EXPECT_EQ(pattern_len,
              ovs_aio_return(ctx,
                             &r_aio));

    EXPECT_EQ(0,
              ovs_aio_wait_completion(r_completion, NULL));
    EXPECT_EQ(0,
              ovs_aio_release_completion(r_completion));

    EXPECT_EQ(0,
              ovs_aio_finish(ctx, &r_aio));

    EXPECT_TRUE(memcmp(rbuf.get(),
                       pattern.c_str(),
                       pattern_len) == 0);
    rbuf.reset();

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

} //namespace
