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
#include <youtils/System.h>
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
namespace yt = youtils;

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

        net_xio_server_ = std::make_unique<NetworkXioInterface>(make_edge_config_(pt,
										  local_node_id()),
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
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
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
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));

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
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));

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
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
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
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
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
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    EXPECT_EQ(0,ovs_ctx_init(ctx,
                             "volume",
                             O_RDWR));

    const size_t bufsize(yt::System::get_env_with_default("EDGE_TEST_BUF_SIZE",
							  4ULL << 10));

    std::vector<char> vec(bufsize);
    {
        const std::string p("openvstorage1");
	for (size_t i = 0; i < vec.size(); i += p.size())
        {
	    snprintf(vec.data() + i,
		     std::min(p.size(),
			      vec.size() - i),
		     "%s",
		     p.c_str());
	}
    }

    for (size_t i = 0; i < 100; ++i)
    {
	struct ovs_aiocb w_aio;
	w_aio.aio_nbytes = vec.size();
	w_aio.aio_offset = 0;
	w_aio.aio_buf = reinterpret_cast<uint8_t*>(vec.data());

	EXPECT_EQ(0,
		  ovs_aio_write(ctx,
				&w_aio));

	EXPECT_EQ(0,
		  ovs_aio_suspend(ctx,
				  &w_aio,
				  NULL));

	EXPECT_EQ(vec.size(),
		  ovs_aio_return(ctx,
				 &w_aio));

	EXPECT_EQ(0,
		  ovs_aio_finish(ctx, &w_aio));

	auto rbuf = std::make_unique<uint8_t[]>(vec.size());
	ASSERT_TRUE(rbuf != nullptr);

	struct ovs_aiocb r_aio;
	r_aio.aio_nbytes = vec.size();
	r_aio.aio_offset = 0;
	r_aio.aio_buf = rbuf.get();

	EXPECT_EQ(0,
		  ovs_aio_read(ctx,
			       &r_aio));

	EXPECT_EQ(0,
		  ovs_aio_suspend(ctx,
				  &r_aio,
				  NULL));

	EXPECT_EQ(vec.size(),
		  ovs_aio_return(ctx,
				 &r_aio));

	EXPECT_EQ(0,
		  ovs_aio_finish(ctx, &r_aio));

	EXPECT_EQ(0,
		  memcmp(rbuf.get(),
			 vec.data(),
			 vec.size()));
    }

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
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
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

TEST_F(NetworkServerTest, stat)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_create_volume(ctx,
                                "volume",
                                volume_size),
              0);
    EXPECT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

    struct stat st;
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, list_volumes)
{
    uint64_t volume_size = 1 << 30;
    size_t max_size = 1;
    int len = -1;
    char *names = NULL;
    std::string vol1("volume1");
    std::string vol2("volume2");

    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);

    {
        names = (char *) malloc(max_size);
        EXPECT_EQ(ovs_list_volumes(ctx, names, &max_size),
                  0);
        free(names);
    }

    EXPECT_EQ(ovs_create_volume(ctx,
                                vol1.c_str(),
                                volume_size),
              0);

    EXPECT_EQ(ovs_create_volume(ctx,
                                vol2.c_str(),
                                volume_size),
              0);

    while (true)
    {
        names = (char *)malloc(max_size);
        len = ovs_list_volumes(ctx, names, &max_size);
        if (len >= 0)
        {
            break;
        }
        if (len == -1 && errno != ERANGE)
        {
            break;
        }
        free(names);
    }

    EXPECT_EQ(len, vol1.length() + 1 + vol2.length() + 1);
    free(names);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, completion_two_ctxs)
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
    };

    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
    ovs_ctx_t *ctx1 = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx1 != nullptr);
    ovs_ctx_t *ctx2 = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx2 != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx1,
                                "volume1",
                                volume_size));

    EXPECT_EQ(0,
              ovs_create_volume(ctx2,
                                "volume2",
                                volume_size));

    EXPECT_EQ(0,
              ovs_ctx_init(ctx1, "volume1", O_RDWR));

    EXPECT_EQ(0,
              ovs_ctx_init(ctx2, "volume2", O_RDWR));

    std::string pattern("openvstorage1");
    ssize_t pattern_len = pattern.length();

    auto w1_buf = std::make_unique<uint8_t[]>(pattern_len);
    ASSERT_TRUE(w1_buf != nullptr);

    auto w2_buf = std::make_unique<uint8_t[]>(pattern_len);
    ASSERT_TRUE(w2_buf != nullptr);

    ovs_completion_t *w1_completion =
        ovs_aio_create_completion(completion_function::finish_write,
                                  &pattern_len);

    ovs_completion_t *w2_completion =
        ovs_aio_create_completion(completion_function::finish_write,
                                  &pattern_len);

    memcpy(w1_buf.get(),
           pattern.c_str(),
           pattern_len);

    memcpy(w2_buf.get(),
           pattern.c_str(),
           pattern_len);

    struct ovs_aiocb w1_aio;
    w1_aio.aio_nbytes = pattern_len;
    w1_aio.aio_offset = 0;
    w1_aio.aio_buf = w1_buf.get();

    struct ovs_aiocb w2_aio;
    w2_aio.aio_nbytes = pattern_len;
    w2_aio.aio_offset = 0;
    w2_aio.aio_buf = w2_buf.get();

    EXPECT_EQ(0,
              ovs_aio_writecb(ctx1,
                              &w1_aio,
                              w1_completion));

    EXPECT_EQ(0,
              ovs_aio_writecb(ctx2,
                              &w2_aio,
                              w2_completion));

    EXPECT_EQ(0,
              ovs_aio_suspend(ctx1,
                              &w1_aio,
                              NULL));

    EXPECT_EQ(pattern_len,
              ovs_aio_return(ctx1,
                             &w1_aio));
    EXPECT_EQ(0,
              ovs_aio_wait_completion(w1_completion, NULL));
    EXPECT_EQ(0,
              ovs_aio_release_completion(w1_completion));

    EXPECT_EQ(0,
              ovs_aio_finish(ctx1, &w1_aio));

    EXPECT_EQ(0,
              ovs_aio_suspend(ctx2,
                              &w2_aio,
                              NULL));

    EXPECT_EQ(pattern_len,
              ovs_aio_return(ctx2,
                             &w2_aio));

    EXPECT_EQ(0,
              ovs_aio_wait_completion(w2_completion, NULL));
    EXPECT_EQ(0,
              ovs_aio_release_completion(w2_completion));

    EXPECT_EQ(0,
              ovs_aio_finish(ctx2, &w2_aio));

    w1_buf.reset();
    w2_buf.reset();

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
              ovs_aio_readcb(ctx2,
                             &r_aio,
                             r_completion));

    EXPECT_EQ(0,
              ovs_aio_suspend(ctx2,
                              &r_aio,
                              NULL));

    EXPECT_EQ(pattern_len,
              ovs_aio_return(ctx2,
                             &r_aio));

    EXPECT_TRUE(memcmp(rbuf.get(),
                       pattern.c_str(),
                       pattern_len) == 0);

    EXPECT_EQ(0,
              ovs_aio_wait_completion(r_completion, NULL));
    EXPECT_EQ(0,
              ovs_aio_release_completion(r_completion));

    EXPECT_EQ(0,
              ovs_aio_finish(ctx2, &r_aio));

    rbuf.reset();

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx1));
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx2));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, write_flush_read)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    EXPECT_EQ(0,
              ovs_ctx_init(ctx,
                           "volume",
                           O_RDWR));

    std::string pattern("openvstorage1");
    auto wbuf = std::make_unique<uint8_t[]>(pattern.length());
    ASSERT_TRUE(wbuf != nullptr);

    memcpy(wbuf.get(),
           pattern.c_str(),
           pattern.length());

    EXPECT_EQ(pattern.length(),
              ovs_write(ctx,
                        wbuf.get(),
                        pattern.length(),
                        1024));

    EXPECT_EQ(0, ovs_flush(ctx));

    wbuf.reset();

    auto rbuf = std::make_unique<uint8_t[]>(pattern.length());
    ASSERT_TRUE(rbuf != nullptr);

    EXPECT_EQ(pattern.length(),
              ovs_read(ctx,
                       rbuf.get(),
                       pattern.length(),
                       1024));

    EXPECT_TRUE(memcmp(rbuf.get(),
                       pattern.c_str(),
                       pattern.length()) == 0);

    rbuf.reset();

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, create_rollback_list_remove_snapshot)
{
    uint64_t volume_size = 1 << 30;
    int64_t timeout = 10;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_create_volume(ctx,
                                "volume",
                                volume_size),
              0);

    EXPECT_EQ(-1,
              ovs_snapshot_create(ctx,
                                  "fvolume",
                                  "snap1",
                                  timeout));
    EXPECT_EQ(ENOENT,
              errno);

    EXPECT_EQ(0,
              ovs_snapshot_create(ctx,
                                  "volume",
                                  "snap1",
                                  timeout));

    EXPECT_EQ(0,
              ovs_snapshot_create(ctx,
                                  "volume",
                                  "snap2",
                                  0));

    EXPECT_EQ(1,
              ovs_snapshot_is_synced(ctx,
                                     "volume",
                                     "snap2"));

    EXPECT_EQ(-1,
              ovs_snapshot_is_synced(ctx,
                                     "volume",
                                     "fsnap"));

    int max_snaps = 1;
    ovs_snapshot_info_t *snaps;
    int snap_count;

    do {
        snaps = (ovs_snapshot_info_t *) malloc(sizeof(*snaps) * max_snaps);
        snap_count = ovs_snapshot_list(ctx, "volume", snaps, &max_snaps);
        if (snap_count <= 0)
        {
            free(snaps);
        }
    } while (snap_count == -1 && errno == ERANGE);

    EXPECT_EQ(2U, snap_count);
    EXPECT_EQ(3U, max_snaps);

    if (snap_count > 0)
    {
        ovs_snapshot_list_free(snaps);
    }

    free(snaps);

    snaps = (ovs_snapshot_info_t *) malloc(sizeof(*snaps));
    max_snaps = 1;

    EXPECT_EQ(-1,
              ovs_snapshot_list(ctx, "fvolume", snaps, &max_snaps));
    EXPECT_EQ(ENOENT,
              errno);

    free(snaps);

    EXPECT_EQ(0,
              ovs_snapshot_rollback(ctx,
                                    "volume",
                                    "snap1"));

    EXPECT_EQ(-1,
              ovs_snapshot_remove(ctx,
                                  "volume",
                                  "snap2"));
    EXPECT_EQ(ENOENT,
              errno);

    EXPECT_EQ(0,
              ovs_snapshot_remove(ctx,
                                  "volume",
                                  "snap1"));

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

} //namespace
