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
#include "../ShmOrbInterface.h"

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

namespace volumedriverfstest
{

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;

using namespace volumedriverfs;

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
    {}

    virtual void
    SetUp()
    {
        FileSystemTestBase::SetUp();
        bpt::ptree pt;
        // simply use the defaults
        shm_orb_server_ = std::make_unique<ShmOrbInterface>(pt,
                                                            RegisterComponent::F,
                                                            *fs_);

        std::promise<void> promise;
        std::future<void> future(promise.get_future());

        shm_orb_thread_ = boost::thread([&]
                                        {
                                            ASSERT_NO_THROW(shm_orb_server_->run(std::move(promise)));
                                        });

        ASSERT_NO_THROW(future.get());

        ShmClient::init();
    }

    virtual void
    TearDown()
    {
        shm_orb_server_->stop_all_and_exit();
        shm_orb_thread_.join();
        shm_orb_server_ = nullptr;
        ShmClient::fini();
        FileSystemTestBase::TearDown();
    }

    std::unique_ptr<ShmOrbInterface> shm_orb_server_;
    boost::thread shm_orb_thread_;
};

TEST_F(ShmServerTest, ovs_create_destroy_context)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(0,
              ovs_create_volume("volume",
                                volume_size));
    ovs_ctx_t *ctx = ovs_ctx_init("volume", O_RDWR);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
}

TEST_F(ShmServerTest, ovs_non_existent_volume)
{
    ovs_ctx_t *ctx = ovs_ctx_init("volume", O_RDWR);
    EXPECT_TRUE(ctx == nullptr);
    EXPECT_EQ(EACCES, errno);
}

TEST_F(ShmServerTest, ovs_remove_non_existent_volume)
{
    EXPECT_EQ(-1,
              ovs_remove_volume("volume"));
    EXPECT_EQ(ENOENT, errno);
}

TEST_F(ShmServerTest, ovs_already_created_volume)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(0,
              ovs_create_volume("volume",
                                volume_size));
    EXPECT_EQ(-1,
              ovs_create_volume("volume",
                                volume_size));
    EXPECT_EQ(EEXIST, errno);
    EXPECT_EQ(0,
              ovs_remove_volume("volume"));
    EXPECT_EQ(-1,
              ovs_remove_volume("volume"));
    EXPECT_EQ(ENOENT, errno);
}

TEST_F(ShmServerTest, ovs_open_same_volume_twice)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(0,
              ovs_create_volume("volume",
                                volume_size));
    ovs_ctx_t *ctx1 = ovs_ctx_init("volume",
                                   O_RDWR);
    ASSERT_TRUE(ctx1 != nullptr);
    ovs_ctx_t *ctx2 = ovs_ctx_init("volume",
                                   O_RDWR);
    ASSERT_TRUE(ctx2 == nullptr);
    EXPECT_EQ(EBUSY, errno);
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx1));
}

TEST_F(ShmServerTest, ovs_create_write_read_destroy)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(0,
              ovs_create_volume("volume",
                                volume_size));
    ovs_ctx_t *ctx = ovs_ctx_init("volume",
                                  O_RDWR);
    ASSERT_TRUE(ctx != nullptr);

    std::string pattern("openvstorage1");
    ovs_buffer_t *wbuf = ovs_allocate(ctx,
                              pattern.length());

    ASSERT_TRUE(wbuf != nullptr);

    memcpy(ovs_buffer_data(wbuf),
           pattern.c_str(),
           pattern.length());

    struct ovs_aiocb w_aio;
    w_aio.aio_nbytes = pattern.length();
    w_aio.aio_offset = 0;
    w_aio.aio_buf = ovs_buffer_data(wbuf);

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

    EXPECT_EQ(0,
              ovs_deallocate(ctx,
                             wbuf));

    ovs_buffer_t *rbuf = ovs_allocate(ctx,
                              pattern.length());

    ASSERT_TRUE(rbuf != nullptr);

    struct ovs_aiocb r_aio;
    r_aio.aio_nbytes = pattern.length();
    r_aio.aio_offset = 0;
    r_aio.aio_buf = ovs_buffer_data(rbuf);

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

    EXPECT_TRUE(memcmp(ovs_buffer_data(rbuf),
                       pattern.c_str(),
                       pattern.length()) == 0);

    EXPECT_EQ(0,
              ovs_deallocate(ctx,
                             rbuf));

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
}

TEST_F(ShmServerTest, ovs_completion)
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
    EXPECT_EQ(0,
              ovs_create_volume("volume",
                                volume_size));
    ovs_ctx_t *ctx = ovs_ctx_init("volume", O_RDWR);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));

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

    memcpy(ovs_buffer_data(wbuf),
           pattern.c_str(),
           pattern_len);

    struct ovs_aiocb w_aio;
    w_aio.aio_nbytes = pattern_len;
    w_aio.aio_offset = 0;
    w_aio.aio_buf = ovs_buffer_data(wbuf);

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

    EXPECT_EQ(0,
              ovs_deallocate(ctx,
                             wbuf));


    ovs_buffer_t *rbuf = ovs_allocate(ctx,
                              pattern_len);

    ASSERT_TRUE(rbuf != nullptr);

    struct ovs_aiocb r_aio;
    r_aio.aio_nbytes = pattern_len;
    r_aio.aio_offset = 0;
    r_aio.aio_buf = ovs_buffer_data(rbuf);

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

    EXPECT_TRUE(memcmp(ovs_buffer_data(rbuf),
                       pattern.c_str(),
                       pattern_len) == 0);

    EXPECT_EQ(0,
              ovs_deallocate(ctx,
                             rbuf));

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
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
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
}

TEST_F(ShmServerTest, ovs_list_volumes)
{
    uint64_t volume_size = 1 << 30;
    size_t max_size = 1;
    int len = -1;
    char *names = NULL;
    std::string vol1("volume1");
    std::string vol2("volume2");

    {
        names = (char *) malloc(max_size);
        EXPECT_EQ(ovs_list_volumes(names, &max_size),
                  0);
        free(names);
    }

    EXPECT_EQ(ovs_create_volume(vol1.c_str(),
                                volume_size),
              0);

    EXPECT_EQ(ovs_create_volume(vol2.c_str(),
                                volume_size),
              0);

    while (true)
    {
        names = (char *)malloc(max_size);
        len = ovs_list_volumes(names, &max_size);
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
}

TEST_F(ShmServerTest, ovs_create_rollback_list_remove_snapshot)
{
    uint64_t volume_size = 1 << 30;
    int64_t timeout = 10;
    EXPECT_EQ(ovs_create_volume("volume",
                                volume_size),
              0);

    EXPECT_EQ(-1,
              ovs_snapshot_create("fvolume",
                                  "snap1",
                                  timeout));
    EXPECT_EQ(ENOENT,
              errno);

    EXPECT_EQ(0,
              ovs_snapshot_create("volume",
                                  "snap1",
                                  timeout));

    EXPECT_EQ(0,
              ovs_snapshot_create("volume",
                                  "snap2",
                                  0));

    EXPECT_EQ(1,
              ovs_snapshot_is_synced("volume",
                                     "snap2"));

    EXPECT_EQ(-1,
              ovs_snapshot_is_synced("volume",
                                     "fsnap"));

    int max_snaps = 1;
    ovs_snapshot_info_t *snaps;
    int snap_count;

    do {
        snaps = (ovs_snapshot_info_t *) malloc(sizeof(*snaps) * max_snaps);
        snap_count = ovs_snapshot_list("volume", snaps, &max_snaps);
        if (snap_count <= 0)
        {
            free(snaps);
        }
    } while (snap_count == -1 && errno == ERANGE);

    EXPECT_EQ(2U, snap_count);
    EXPECT_EQ(3U, max_snaps);

    ovs_snapshot_list_free(snaps);

    EXPECT_EQ(-1,
              ovs_snapshot_list("fvolume", snaps, &max_snaps));

    EXPECT_EQ(ENOENT,
              errno);

    free(snaps);

    EXPECT_EQ(0,
              ovs_snapshot_rollback("volume",
                                    "snap1"));

    EXPECT_EQ(-1,
              ovs_snapshot_remove("volume",
                                  "snap2"));
    EXPECT_EQ(ENOENT,
              errno);

    EXPECT_EQ(0,
              ovs_snapshot_remove("volume",
                                  "snap1"));
}

TEST_F(ShmServerTest, ovs_completion_two_ctxs)
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
    EXPECT_EQ(0,
              ovs_create_volume("volume1",
                                volume_size));

    EXPECT_EQ(0,
              ovs_create_volume("volume2",
                                volume_size));

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

    memcpy(ovs_buffer_data(w1_buf),
           pattern.c_str(),
           pattern_len);

    memcpy(ovs_buffer_data(w2_buf),
           pattern.c_str(),
           pattern_len);

    struct ovs_aiocb w1_aio;
    w1_aio.aio_nbytes = pattern_len;
    w1_aio.aio_offset = 0;
    w1_aio.aio_buf = ovs_buffer_data(w1_buf);

    struct ovs_aiocb w2_aio;
    w2_aio.aio_nbytes = pattern_len;
    w2_aio.aio_offset = 0;
    w2_aio.aio_buf = ovs_buffer_data(w2_buf);

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

    EXPECT_EQ(0,
              ovs_deallocate(ctx1,
                             w1_buf));
    EXPECT_EQ(0,
              ovs_deallocate(ctx2,
                             w2_buf));

    ovs_buffer_t *rbuf = ovs_allocate(ctx2,
                              pattern_len);

    struct ovs_aiocb r_aio;
    r_aio.aio_nbytes = pattern_len;
    r_aio.aio_offset = 0;
    r_aio.aio_buf = ovs_buffer_data(rbuf);

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

    EXPECT_TRUE(memcmp(ovs_buffer_data(rbuf),
                       pattern.c_str(),
                       pattern_len) == 0);

    EXPECT_EQ(0,
              ovs_aio_wait_completion(r_completion, NULL));
    EXPECT_EQ(0,
              ovs_aio_release_completion(r_completion));

    EXPECT_EQ(0,
              ovs_aio_finish(ctx2, &r_aio));

    EXPECT_EQ(0,
              ovs_deallocate(ctx2,
                             rbuf));

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx1));
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx2));
}

TEST_F(ShmServerTest, ovs_write_flush_read)
{
    uint64_t volume_size = 1 << 30;
    EXPECT_EQ(0,
              ovs_create_volume("volume",
                                volume_size));
    ovs_ctx_t *ctx = ovs_ctx_init("volume",
                                  O_RDWR);
    ASSERT_TRUE(ctx != nullptr);

    std::string pattern("openvstorage1");
    ovs_buffer_t *wbuf = ovs_allocate(ctx,
                              pattern.length());

    ASSERT_TRUE(wbuf != nullptr);

    memcpy(ovs_buffer_data(wbuf),
           pattern.c_str(),
           pattern.length());

    EXPECT_EQ(pattern.length(),
              ovs_write(ctx,
                        ovs_buffer_data(wbuf),
                        pattern.length(),
                        1024));

    EXPECT_EQ(0, ovs_flush(ctx));

    EXPECT_EQ(0,
              ovs_deallocate(ctx,
                             wbuf));

    ovs_buffer_t *rbuf = ovs_allocate(ctx,
                              pattern.length());

    ASSERT_TRUE(rbuf != nullptr);

    EXPECT_EQ(pattern.length(),
              ovs_read(ctx,
                       ovs_buffer_data(rbuf),
                       pattern.length(),
                       1024));

    EXPECT_TRUE(memcmp(ovs_buffer_data(rbuf),
                       pattern.c_str(),
                       pattern.length()) == 0);

    EXPECT_EQ(0,
              ovs_deallocate(ctx,
                             rbuf));

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
}

} //namespace volumedriverfstest
