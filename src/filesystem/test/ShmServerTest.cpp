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
#include <filesystem/ShmClient.h>
#include <filesystem/c-api/volumedriver.h>

#include "../PythonClient.h"

namespace volumedriverfstest
{

namespace fs = boost::filesystem;
namespace vfs = volumedriverfs;

class ShmServerTest
    : public FileSystemTestBase
{
public:
    ShmServerTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("ShmServerTest")
                             .redirect_timeout_ms(10000)
                             .backend_sync_timeout_ms(9500)
                             .migrate_timeout_ms(500)
                             .redirect_retries(1))
        , remote_root_(mount_dir(remote_dir(topdir_)))
    {}

    virtual void
    SetUp()
    {
        FileSystemTestBase::SetUp();
        start_failovercache_for_remote_node();
        mount_remote();
    }

    virtual void
    TearDown()
    {
        umount_remote();
        stop_failovercache_for_remote_node();
        FileSystemTestBase::TearDown();
    }

    const fs::path remote_root_;
};

TEST_F(ShmServerTest, ovs_create_destroy_context)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(ovs_create_volume("volume",
                                volume_size),
              0);
    ovs_ctx_t *ctx = ovs_ctx_init("volume", O_RDWR);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_ctx_destroy(ctx),
              0);
}

TEST_F(ShmServerTest, ovs_non_existent_volume)
{
    ovs_ctx_t *ctx = ovs_ctx_init("volume", O_RDWR);
    EXPECT_TRUE(ctx == nullptr);
    EXPECT_EQ(errno, EACCES);
}

TEST_F(ShmServerTest, ovs_already_created_volume)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(ovs_create_volume("volume",
                                volume_size),
              0);
    EXPECT_EQ(ovs_create_volume("volume",
                                volume_size),
             -1);
    EXPECT_EQ(errno, EEXIST);
}

TEST_F(ShmServerTest, ovs_create_write_read_destroy)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(ovs_create_volume("volume",
                                volume_size),
              0);
    ovs_ctx_t *ctx = ovs_ctx_init("volume",
                                  O_RDWR);
    ASSERT_TRUE(ctx != nullptr);

    std::string pattern("openvstorage1");
    ovs_buffer_t *wbuf = ovs_allocate(ctx,
                              pattern.length());

    ASSERT_TRUE(wbuf != nullptr);

    memcpy(wbuf->buf,
           pattern.c_str(),
           pattern.length());

    struct ovs_aiocb w_aio;
    w_aio.aio_nbytes = pattern.length();
    w_aio.aio_offset = 0;
    w_aio.aio_buf = wbuf->buf;

    EXPECT_EQ(ovs_aio_write(ctx,
                            &w_aio),
              0);

    EXPECT_EQ(ovs_aio_suspend(ctx,
                              &w_aio,
                              NULL),
              0);

    EXPECT_EQ(ovs_aio_return(ctx,
                             &w_aio),
              pattern.length());

    ovs_deallocate(ctx,
                   wbuf);

    ovs_buffer_t *rbuf = ovs_allocate(ctx,
                              pattern.length());

    ASSERT_TRUE(rbuf != nullptr);

    struct ovs_aiocb r_aio;
    r_aio.aio_nbytes = pattern.length();
    r_aio.aio_offset = 0;
    r_aio.aio_buf = rbuf->buf;

    EXPECT_EQ(ovs_aio_read(ctx,
                           &r_aio),
              0);

    EXPECT_EQ(ovs_aio_suspend(ctx,
                              &r_aio,
                              NULL),
              0);

    EXPECT_EQ(ovs_aio_return(ctx,
                             &r_aio),
              pattern.length());

    EXPECT_TRUE(memcmp(rbuf->buf,
                       pattern.c_str(),
                       pattern.length()) == 0);

    ovs_deallocate(ctx,
                   rbuf);

    EXPECT_EQ(ovs_ctx_destroy(ctx),
              0);
}

TEST_F(ShmServerTest, ovs_completion)
{
    struct completion_function
    {
        static void finish_write(ovs_completion_t *comp, void *arg)
        {
            ssize_t *len = (ssize_t*)arg;
            EXPECT_EQ(ovs_aio_return_completion(comp),
                      *len);
            EXPECT_EQ(ovs_aio_release_completion(comp),
                      0);
        }
        static void finish_read(ovs_completion_t *comp, void *arg)
        {
            ssize_t *len = (ssize_t*)arg;
            EXPECT_EQ(ovs_aio_return_completion(comp),
                      *len);
            EXPECT_EQ(ovs_aio_release_completion(comp),
                      0);
        }
        static void finish_flush(ovs_completion_t *comp, void * /*arg*/)
        {
            EXPECT_EQ(ovs_aio_return_completion(comp),
                      0);
            EXPECT_EQ(ovs_aio_release_completion(comp),
                      0);
        }
    };

    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(ovs_create_volume("volume",
                                volume_size),
              0);
    ovs_ctx_t *ctx = ovs_ctx_init("volume", O_RDWR);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_ctx_destroy(ctx),
              0);

    ctx = ovs_ctx_init("volume", O_RDWR);
    ASSERT_TRUE(ctx != nullptr);

    std::string pattern("openvstorage1");
    ssize_t pattern_len = pattern.length();
    ovs_buffer_t *wbuf = ovs_allocate(ctx,
                              pattern_len);

    ASSERT_TRUE(wbuf != nullptr);

    ovs_completion_t *w_completion =
        ovs_aio_create_completion(completion_function::finish_write,
                                  &pattern_len);

    ovs_completion_t *f_completion =
        ovs_aio_create_completion(completion_function::finish_flush,
                                  NULL);

    memcpy(wbuf->buf,
           pattern.c_str(),
           pattern_len);

    struct ovs_aiocb w_aio;
    w_aio.aio_nbytes = pattern_len;
    w_aio.aio_offset = 0;
    w_aio.aio_buf = wbuf->buf;

    EXPECT_EQ(ovs_aio_writecb(ctx,
                              &w_aio,
                              w_completion),
              0);

    EXPECT_EQ(ovs_aio_flushcb(ctx,
                              f_completion),
              0);

    EXPECT_EQ(ovs_aio_suspend(ctx,
                              &w_aio,
                              NULL),
              0);

    EXPECT_EQ(ovs_aio_return(ctx,
                             &w_aio),
              pattern_len);

    ovs_deallocate(ctx,
                   wbuf);

    ovs_buffer_t *rbuf = ovs_allocate(ctx,
                              pattern_len);

    ASSERT_TRUE(rbuf != nullptr);

    struct ovs_aiocb r_aio;
    r_aio.aio_nbytes = pattern_len;
    r_aio.aio_offset = 0;
    r_aio.aio_buf = rbuf->buf;

    ovs_completion_t *r_completion =
        ovs_aio_create_completion(completion_function::finish_read,
                                  &pattern_len);

    EXPECT_EQ(ovs_aio_readcb(ctx,
                             &r_aio,
                             r_completion),
              0);

    EXPECT_EQ(ovs_aio_suspend(ctx,
                              &r_aio,
                              NULL),
              0);

    EXPECT_EQ(ovs_aio_return(ctx,
                             &r_aio),
              pattern_len);

    EXPECT_TRUE(memcmp(rbuf->buf,
                       pattern.c_str(),
                       pattern_len) == 0);

    ovs_deallocate(ctx,
                   rbuf);

    EXPECT_EQ(ovs_ctx_destroy(ctx),
              0);
}

TEST_F(ShmServerTest, ovs_stat)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(ovs_create_volume("volume",
                                volume_size),
              0);
    ovs_ctx_t *ctx = ovs_ctx_init("volume", O_RDWR);
    ASSERT_TRUE(ctx != nullptr);

    struct stat st;
    EXPECT_EQ(ovs_stat(ctx, &st),
              0);
    EXPECT_EQ(st.st_size,
              volume_size);

    EXPECT_EQ(ovs_ctx_destroy(ctx),
              0);
}

TEST_F(ShmServerTest, ovs_completion_two_ctxs)
{
    struct completion_function
    {
        static void finish_write(ovs_completion_t *comp, void *arg)
        {
            ssize_t *len = (ssize_t*)arg;
            EXPECT_EQ(ovs_aio_return_completion(comp),
                      *len);
            EXPECT_EQ(ovs_aio_release_completion(comp),
                      0);
        }
        static void finish_read(ovs_completion_t *comp, void *arg)
        {
            ssize_t *len = (ssize_t*)arg;
            EXPECT_EQ(ovs_aio_return_completion(comp),
                      *len);
            EXPECT_EQ(ovs_aio_release_completion(comp),
                      0);
        }
    };

    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(ovs_create_volume("volume1",
                                volume_size),
              0);

    EXPECT_EQ(ovs_create_volume("volume2",
                                volume_size),
              0);

    ovs_ctx_t *ctx1 = ovs_ctx_init("volume1", O_RDWR);
    ASSERT_TRUE(ctx1 != nullptr);

    ovs_ctx_t *ctx2 = ovs_ctx_init("volume2", O_RDWR);
    ASSERT_TRUE(ctx2 != nullptr);

    std::string pattern("openvstorage1");
    ssize_t pattern_len = pattern.length();

    ovs_buffer_t *w1_buf = ovs_allocate(ctx1,
                                pattern_len);

    ASSERT_TRUE(w1_buf != nullptr);

    ovs_buffer_t *w2_buf = ovs_allocate(ctx2,
                                pattern_len);

    ASSERT_TRUE(w2_buf != nullptr);

    ovs_completion_t *w1_completion =
        ovs_aio_create_completion(completion_function::finish_write,
                                  &pattern_len);

    ovs_completion_t *w2_completion =
        ovs_aio_create_completion(completion_function::finish_write,
                                  &pattern_len);

    memcpy(w1_buf->buf,
           pattern.c_str(),
           pattern_len);

    memcpy(w2_buf->buf,
           pattern.c_str(),
           pattern_len);

    struct ovs_aiocb w1_aio;
    w1_aio.aio_nbytes = pattern_len;
    w1_aio.aio_offset = 0;
    w1_aio.aio_buf = w1_buf->buf;

    struct ovs_aiocb w2_aio;
    w2_aio.aio_nbytes = pattern_len;
    w2_aio.aio_offset = 0;
    w2_aio.aio_buf = w2_buf->buf;

    EXPECT_EQ(ovs_aio_writecb(ctx1,
                              &w1_aio,
                              w1_completion),
              0);

    EXPECT_EQ(ovs_aio_writecb(ctx2,
                              &w2_aio,
                              w2_completion),
              0);

    EXPECT_EQ(ovs_aio_suspend(ctx1,
                              &w1_aio,
                              NULL),
              0);

    EXPECT_EQ(ovs_aio_return(ctx1,
                             &w1_aio),
              pattern_len);

    EXPECT_EQ(ovs_aio_suspend(ctx2,
                              &w2_aio,
                              NULL),
              0);

    EXPECT_EQ(ovs_aio_return(ctx2,
                             &w2_aio),
              pattern_len);

    ovs_deallocate(ctx1,
                   w1_buf);
    ovs_deallocate(ctx2,
                   w2_buf);

    ovs_buffer_t *rbuf = ovs_allocate(ctx2,
                              pattern_len);

    struct ovs_aiocb r_aio;
    r_aio.aio_nbytes = pattern_len;
    r_aio.aio_offset = 0;
    r_aio.aio_buf = rbuf->buf;

    ovs_completion_t *r_completion =
        ovs_aio_create_completion(completion_function::finish_read,
                                  &pattern_len);

    EXPECT_EQ(ovs_aio_readcb(ctx2,
                             &r_aio,
                             r_completion),
              0);

    EXPECT_EQ(ovs_aio_suspend(ctx2,
                              &r_aio,
                              NULL),
              0);

    EXPECT_EQ(ovs_aio_return(ctx2,
                             &r_aio),
              pattern_len);

    EXPECT_TRUE(memcmp(rbuf->buf,
                       pattern.c_str(),
                       pattern_len) == 0);

    ovs_deallocate(ctx2,
                   rbuf);

    EXPECT_EQ(ovs_ctx_destroy(ctx1),
              0);
    EXPECT_EQ(ovs_ctx_destroy(ctx2),
              0);
}

TEST_F(ShmServerTest, ovs_write_flush_read)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(ovs_create_volume("volume",
                                volume_size),
              0);
    ovs_ctx_t *ctx = ovs_ctx_init("volume",
                                  O_RDWR);
    ASSERT_TRUE(ctx != nullptr);

    std::string pattern("openvstorage1");
    ovs_buffer_t *wbuf = ovs_allocate(ctx,
                              pattern.length());

    ASSERT_TRUE(wbuf != nullptr);

    memcpy(wbuf->buf,
           pattern.c_str(),
           pattern.length());

    EXPECT_EQ(ovs_write(ctx,
                        wbuf->buf,
                        pattern.length(),
                        1024),
              pattern.length());

    EXPECT_EQ(ovs_flush(ctx), 0);

    ovs_deallocate(ctx,
                   wbuf);

    ovs_buffer_t *rbuf = ovs_allocate(ctx,
                              pattern.length());

    ASSERT_TRUE(rbuf != nullptr);

    EXPECT_EQ(ovs_read(ctx,
                       rbuf->buf,
                       pattern.length(),
                       1024),
              pattern.length());

    EXPECT_TRUE(memcmp(rbuf->buf,
                       pattern.c_str(),
                       pattern.length()) == 0);

    ovs_deallocate(ctx,
                   rbuf);

    EXPECT_EQ(ovs_ctx_destroy(ctx),
              0);
}

} //namespace volumedriverfstest
