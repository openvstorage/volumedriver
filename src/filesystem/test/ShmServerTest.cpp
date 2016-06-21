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

#include "../PythonClient.h"
#include "../ShmOrbInterface.h"

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
#include <filesystem/c-api/ShmClient.h>
#include <filesystem/c-api/volumedriver.h>

namespace volumedriverfstest
{

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace yt = youtils;

using namespace volumedriverfs;
using namespace std::literals::string_literals;

namespace
{
struct CtxAttrDeleter
{
    void
    operator()(ovs_ctx_attr_t* a)
    {
        ovs_ctx_attr_destroy(a);
    }
};

using CtxAttrPtr = std::unique_ptr<ovs_ctx_attr_t, CtxAttrDeleter>;

struct CtxDeleter
{
    void
    operator()(ovs_ctx_t* c)
    {
        ovs_ctx_destroy(c);
    }
};

using CtxPtr = std::unique_ptr<ovs_ctx_t, CtxDeleter>;

struct BufferDeleter
{
    ovs_context_t& ctx;

    BufferDeleter(ovs_context_t& c)
        : ctx(c)
    {}

    void
    operator()(ovs_buffer_t* b)
    {
        if (b)
        {
            ovs_deallocate(&ctx,
                           b);
        }
    }
};

using BufferPtr = std::unique_ptr<ovs_buffer_t, BufferDeleter>;

}

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

    static std::string
    shm_segment_details(const ClusterId& cid,
                        const NodeId& nid)
    {
        return cid.str() + "/"s + nid.str();
    }

    std::string
    shm_segment_details() const
    {
        return shm_segment_details(vrouter_cluster_id(),
                                   local_node_id());
    }
};

TEST_F(ShmServerTest, ovs_create_destroy_context)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();

    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));

    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));
    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_non_existent_volume)
{
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));

    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(-1,
              ovs_ctx_init(ctx, "volume", O_RDWR));
    EXPECT_EQ(EACCES, errno);
    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_remove_non_existent_volume)
{
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));

    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(-1,
              ovs_remove_volume(ctx, "volume"));
    EXPECT_EQ(ENOENT, errno);
    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_already_created_volume)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
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
    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_open_same_volume_twice)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
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
    EXPECT_EQ(-1,
              ovs_ctx_init(ctx2,
                           "volume",
                           O_RDWR));
    EXPECT_EQ(EBUSY, errno);
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx1));
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx2));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_create_write_read_destroy)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    ASSERT_EQ(0,ovs_ctx_init(ctx,
                             "volume",
                             O_RDWR));

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

    ASSERT_EQ(0,
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

    ASSERT_EQ(0,
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
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
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
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));
    ASSERT_EQ(0,
              ovs_ctx_destroy(ctx));

    ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

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

    ASSERT_EQ(0,
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

    ASSERT_EQ(0,
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
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_stat)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_create_volume(ctx,
                                "volume",
                                volume_size),
              0);
    ASSERT_EQ(0,
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

TEST_F(ShmServerTest, ovs_list_volumes)
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
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));

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

TEST_F(ShmServerTest, ovs_create_rollback_list_remove_snapshot)
{
    uint64_t volume_size = 1 << 30;
    int64_t timeout = 10;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
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
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
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

    ASSERT_EQ(0,
              ovs_ctx_init(ctx1, "volume1", O_RDWR));

    ASSERT_EQ(0,
              ovs_ctx_init(ctx2, "volume2", O_RDWR));

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

    ASSERT_EQ(0,
              ovs_aio_writecb(ctx1,
                              &w1_aio,
                              w1_completion));

    ASSERT_EQ(0,
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

    ASSERT_EQ(0,
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
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_write_flush_read)
{
    uint64_t volume_size = 1 << 30;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(0,
              ovs_create_volume(ctx,
                                "volume",
                                volume_size));
    ASSERT_EQ(0,
              ovs_ctx_init(ctx,
                           "volume",
                           O_RDWR));

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
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_create_truncate_volume)
{
    uint64_t volume_size = 1 << 20;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_create_volume(ctx,
                                "volume",
                                volume_size),
              0);
    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

    struct stat st;
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    uint64_t new_volume_size = 1 << 30;
    EXPECT_EQ(ovs_truncate_volume(ctx,
                                  "volume",
                                  new_volume_size),
              0);
    memset(&st, 0x0, sizeof(struct stat));
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              new_volume_size);

    EXPECT_EQ(ovs_truncate_volume(ctx,
                                  "non_existend_volume",
                                  new_volume_size),
              -1);
    EXPECT_EQ(ENOENT, errno);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_truncate_volume)
{
    uint64_t volume_size = 1 << 20;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_create_volume(ctx,
                                "volume",
                                volume_size),
              0);

    uint64_t new_volume_size = 1 << 30;
    EXPECT_EQ(ovs_truncate(ctx,
                           new_volume_size),
              -1);
    EXPECT_EQ(EBADF, errno);

    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

    struct stat st;
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    EXPECT_EQ(ovs_truncate(ctx,
                           new_volume_size),
              0);
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              new_volume_size);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, ovs_fail_to_truncate_volume)
{
    uint64_t volume_size = 1 << 20;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         "shm",
                                         shm_segment_details().c_str(),
                                         0));

    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_create_volume(ctx,
                                "volume",
                                volume_size),
              0);

    uint64_t new_volume_size = 1 << 30;
    EXPECT_EQ(ovs_truncate(ctx,
                           new_volume_size),
              -1);
    EXPECT_EQ(EBADF, errno);

    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDONLY));

    EXPECT_EQ(ovs_truncate(ctx,
                           new_volume_size),
              -1);
    EXPECT_EQ(EBADF, errno);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(ShmServerTest, invalid_cluster_id)
{
    CtxAttrPtr attr(ovs_ctx_attr_new());
    ASSERT_TRUE(attr != nullptr);

    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(attr.get(),
                                         "shm",
                                         shm_segment_details(ClusterId("foo"),
                                                             local_node_id()).c_str(),
                                         0));

    CtxPtr ctx(ovs_ctx_new(attr.get()));
    ASSERT_TRUE(ctx != nullptr);

    const std::string vname("volume");
    const size_t vsize = 1ULL << 20;
    EXPECT_EQ(-1,
              ovs_create_volume(ctx.get(),
                                vname.c_str(),
                                vsize));
}

TEST_F(ShmServerTest, invalid_node_id)
{
    CtxAttrPtr attr(ovs_ctx_attr_new());
    ASSERT_TRUE(attr != nullptr);

    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(attr.get(),
                                         "shm",
                                         shm_segment_details(vrouter_cluster_id(),
                                                             NodeId("foo")).c_str(),
                                         0));

    CtxPtr ctx(ovs_ctx_new(attr.get()));
    ASSERT_TRUE(ctx != nullptr);

    const std::string vname("volume");
    const size_t vsize = 1ULL << 20;
    EXPECT_EQ(-1,
              ovs_create_volume(ctx.get(),
                                vname.c_str(),
                                vsize));
}

TEST_F(ShmServerTest, two_instances)
{
    const size_t vsize = 1ULL << 20;
    std::atomic<bool> stop(false);

    auto work([&](const ClusterId& cid,
                  const NodeId& nid,
                  const std::string& vid,
                  const std::string& pattern)
              {
                  CtxAttrPtr attr(ovs_ctx_attr_new());
                  ASSERT_TRUE(attr != nullptr);

                  EXPECT_EQ(0,
                            ovs_ctx_attr_set_transport(attr.get(),
                                                       "shm",
                                                       shm_segment_details(cid,
                                                                           nid).c_str(),
                                                       0));

                  CtxPtr ctx(ovs_ctx_new(attr.get()));
                  ASSERT_TRUE(ctx != nullptr);

                  EXPECT_EQ(0,
                            ovs_create_volume(ctx.get(),
                                              vid.c_str(),
                                              vsize));

                  ASSERT_EQ(0,
                            ovs_ctx_init(ctx.get(),
                                         vid.c_str(),
                                         O_RDWR));

                  BufferPtr wbuf(ovs_allocate(ctx.get(),
                                              pattern.size()),
                                 BufferDeleter(*ctx));

                  memcpy(ovs_buffer_data(wbuf.get()),
                         pattern.data(),
                         pattern.size());

                  while (not stop)
                  {
                      EXPECT_EQ(pattern.size(),
                                ovs_write(ctx.get(),
                                          ovs_buffer_data(wbuf.get()),
                                          pattern.size(),
                                          0));

                      BufferPtr rbuf(ovs_allocate(ctx.get(),
                                                  pattern.size()),
                                 BufferDeleter(*ctx));

                      EXPECT_EQ(pattern.size(),
                                ovs_read(ctx.get(),
                                         ovs_buffer_data(rbuf.get()),
                                         pattern.size(),
                                         0));

                      EXPECT_EQ(pattern,
                                std::string(static_cast<const char*>(ovs_buffer_data(rbuf.get())),
                                            pattern.size()));
                  }
              });

    mount_remote();
    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         umount_remote();
                                     }));

    auto rfuture(std::async(std::launch::async,
                            [&]
                            {
                                const auto cid(yt::System::get_env_with_default("SHM_TEST_REMOTE_CLUSTER_ID",
                                                                                vrouter_cluster_id()));
                                const auto nid(yt::System::get_env_with_default("SHM_TEST_REMOTE_NODE_ID",
                                                                                remote_node_id()));
                                const auto vid(yt::System::get_env_with_default("SHM_TEST_REMOTE_VOLUME_ID",
                                                                                "remote-volume"s));
                                work(cid,
                                     nid,
                                     vid,
                                     "remote work"s);
                            }));

    auto lfuture(std::async(std::launch::async,
                            [&]
                            {
                                work(vrouter_cluster_id(),
                                     local_node_id(),
                                     "local-volume"s,
                                     "local work"s);
                            }));

    const auto sleep_secs = yt::System::get_env_with_default("SHM_TEST_DURATION_SECS",
                                                             20UL);

    std::this_thread::sleep_for(std::chrono::seconds(sleep_secs));

    stop = true;

    rfuture.wait();
    lfuture.wait();
}

} //namespace volumedriverfstest
