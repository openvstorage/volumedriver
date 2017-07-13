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
#include "../NetworkXioInterface.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/python/extract.hpp>

#include <youtils/ArakoonInterface.h>
#include <youtils/BooleanEnum.h>
#include <youtils/Catchers.h>
#include <youtils/FileUtils.h>
#include <youtils/FileDescriptor.h>
#include <youtils/System.h>
#include <youtils/wall_timer.h>

#include <volumedriver/Api.h>
#include <volumedriver/CachedMetaDataPage.h>
#include <volumedriver/Volume.h>

#include <filesystem/ObjectRouter.h>
#include <filesystem/Registry.h>

#include <filesystem/c-api/common.h>
#include <filesystem/c-api/context.h>
#include <filesystem/c-api/NetworkHAContext.h>
#include <filesystem/c-api/volumedriver.h>

namespace volumedriverfstest
{

#define LOCKVD()                                        \
    fungi::ScopedLock ag__(api::getManagementMutex())

namespace bc = boost::chrono;
namespace bpt = boost::property_tree;
namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace vd = volumedriver;
namespace vfs = volumedriverfs;
namespace yt = youtils;
namespace libovs = libovsvolumedriver;

using namespace std::literals::string_literals;
using namespace volumedriverfs;

namespace
{

VD_BOOLEAN_ENUM(EnableHa);

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

}

class NetworkServerTest
    : public FileSystemTestBase
{
public:
    NetworkServerTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("NetworkServerTest")
                             .redirect_timeout_ms(10000)
                             .backend_sync_timeout_ms(9500)
                             .migrate_timeout_ms(500)
                             .redirect_retries(1)
                             .dtl_mode(vd::FailOverCacheMode::Synchronous))
        , client_(vrouter_cluster_id(),
                  {{address(), local_config().xmlrpc_port}})
    {
        Py_Initialize();
    }

    ~NetworkServerTest()
    {
        Py_Finalize();
    }

    virtual void
    SetUp()
    {
        FileSystemTestBase::SetUp();
        start_failovercache_for_remote_node();
        bpt::ptree pt;

        net_xio_server_ = std::make_unique<NetworkXioInterface>(pt,
                                                                RegisterComponent::F,
                                                                *fs_);
        std::promise<void> promise;
        std::future<void> future(promise.get_future());

        net_xio_thread_ = boost::thread([&]{
                ASSERT_NO_THROW(net_xio_server_->run(std::move(promise)));
            });

        ASSERT_NO_THROW(future.get());
    }

    virtual void
    TearDown()
    {
        stop_failovercache_for_remote_node();
        if (net_xio_server_)
        {
            net_xio_server_->shutdown();
        }

        net_xio_thread_.join();
        net_xio_server_ = nullptr;

        FileSystemTestBase::TearDown();
    }

    static uint16_t
    edge_port(const bool remote)
    {
        return
            remote ?
            FileSystemTestSetup::remote_edge_port() :
            FileSystemTestSetup::local_edge_port();
    }

    static CtxAttrPtr
    make_ctx_attr(const size_t qdepth,
                  const EnableHa enable_ha,
                  const uint16_t port = FileSystemTestSetup::local_edge_port())
    {
        CtxAttrPtr attr(ovs_ctx_attr_new());
        EXPECT_TRUE(attr != nullptr);

        EXPECT_EQ(0,
                  ovs_ctx_attr_set_transport(attr.get(),
                                             FileSystemTestSetup::edge_transport().c_str(),
                                             FileSystemTestSetup::address().c_str(),
                                             port));

        EXPECT_EQ(0,
                  ovs_ctx_attr_set_network_qdepth(attr.get(),
                                                  qdepth));
        if (enable_ha == EnableHa::T)
        {
            EXPECT_EQ(0,
                      ovs_ctx_attr_enable_ha(attr.get()));
        }
        return attr;
    }

    CtxPtr
    make_volume(const std::string& vname,
                const size_t vsize,
                const uint16_t port,
                const EnableHa enable_ha = EnableHa::T,
                const size_t qdepth = 1024)
    {
        CtxAttrPtr attrs(make_ctx_attr(qdepth,
                                       enable_ha,
                                       port));

        CtxPtr ctx(ovs_ctx_new(attrs.get()));
        EXPECT_TRUE(ctx != nullptr);

        const size_t max = port == FileSystemTestSetup::local_edge_port() ? 0 : 10;
        size_t count = 0;

        // TODO: there's a race with the startup of the network server on
        // the remote instance and a proper fix is out of scope for the moment
        while (ovs_create_volume(ctx.get(),
                                 vname.c_str(),
                                 vsize) == -1)
        {
            // What we *actually* want here is ASSERT_GT but that doesn't work here.
            // Hence throw an exception to end the misery quickly.
            EXPECT_GT(max, ++count) <<
                "failed to create volume after " << count <<
                " attempts: " << strerror(errno);
            if (max <= count)
            {
                throw std::runtime_error("volume creation retries exceeded, aborting test");
            }

            boost::this_thread::sleep_for(bc::milliseconds(250));
        }

        return ctx;
    }

    CtxPtr
    open_volume(const std::string& vname,
                const uint16_t port,
                const int oflag,
                const EnableHa enable_ha = EnableHa::T,
                const size_t qdepth = 1024)
    {
        CtxAttrPtr attrs(make_ctx_attr(qdepth,
                                       enable_ha,
                                       port));
        CtxPtr ctx(ovs_ctx_new(attrs.get()));
        EXPECT_TRUE(ctx != nullptr);

        EXPECT_EQ(0,
                  ovs_ctx_init(ctx.get(),
                               vname.c_str(),
                               oflag));

        return ctx;
    }

    void
    set_max_neighbour_distance(uint32_t dist)
    {
        bpt::ptree pt;
        net_xio_server_->persist(pt, ReportDefault::F);
        ip::PARAMETER_TYPE(network_max_neighbour_distance)(dist).persist(pt);
        yt::UpdateReport urep;
        net_xio_server_->update(pt, urep);
        ASSERT_EQ(1, urep.update_size());
    }

    void
    add_ghost_node_configs(const size_t ghost_nodes,
                           ClusterNodeConfigs& configs)
    {
        configs.reserve(configs.size() + ghost_nodes);

        for (size_t i = 0; i < ghost_nodes; ++i)
        {
            ClusterNodeConfig ghost(configs[0]);
            const std::string n("ghost-node-"s + boost::lexical_cast<std::string>(i));
            ghost.vrouter_id = NodeId(n);
            ghost.network_server_uri = yt::Uri("tcp://"s + n + ":1234"s);
            configs.push_back(ghost);
        }
    }

    ClusterNodeConfigs
    update_cluster_registry(std::function<void(ClusterNodeConfigs&)> fun)
    {
        std::shared_ptr<ClusterRegistry> creg(fs_->object_router().cluster_registry());
        ClusterNodeConfigs configs(creg->get_node_configs());
        fun(configs);
        creg->set_node_configs(configs);
        return configs;
    }

    void
    test_snapshot_ops(const bool remote)
    {
        const std::string vname("volume");

        make_volume(vname,
                    1ULL << 20,
                    edge_port(remote));

        CtxPtr ctx(open_volume(vname,
                               edge_port(false),
                               O_RDWR,
                               EnableHa::F));
        ASSERT_TRUE(ctx != nullptr);

        const int64_t timeout = 10;
        const std::string snap1("snap1");

        EXPECT_EQ(-1,
                  ovs_snapshot_create(ctx.get(),
                                      (vname + "X").c_str(),
                                      snap1.c_str(),
                                      timeout));
        EXPECT_EQ(ENOENT,
                  errno);

        EXPECT_EQ(0,
                  ovs_snapshot_create(ctx.get(),
                                      vname.c_str(),
                                      snap1.c_str(),
                                      timeout));

        const std::string snap2("snap2");

        EXPECT_EQ(0,
                  ovs_snapshot_create(ctx.get(),
                                      vname.c_str(),
                                      snap2.c_str(),
                                      0));

        EXPECT_EQ(1,
                  ovs_snapshot_is_synced(ctx.get(),
                                         vname.c_str(),
                                         snap2.c_str()));

        EXPECT_EQ(-1,
                  ovs_snapshot_is_synced(ctx.get(),
                                         vname.c_str(),
                                         "fsnap"));

        int max_snaps = 1;

        std::vector<ovs_snapshot_info_t> snaps(max_snaps);

        int snap_count = ovs_snapshot_list(ctx.get(),
                                           vname.c_str(),
                                           snaps.data(),
                                           &max_snaps);
        EXPECT_EQ(-1,
                  snap_count);
        EXPECT_EQ(ERANGE,
                  errno);
        EXPECT_LT(1,
                  max_snaps);

        snaps.resize(max_snaps);

        snap_count = ovs_snapshot_list(ctx.get(),
                                           vname.c_str(),
                                           snaps.data(),
                                           &max_snaps);
        ASSERT_EQ(2U, snap_count);
        EXPECT_EQ(3U, max_snaps);

        ovs_snapshot_list_free(snaps.data());

        snaps.resize(1);
        max_snaps = snaps.size();

        EXPECT_EQ(-1,
                  ovs_snapshot_list(ctx.get(),
                                    "fvolume",
                                    snaps.data(),
                                    &max_snaps));
        EXPECT_EQ(ENOENT,
                  errno);

        EXPECT_EQ(0,
                  ovs_snapshot_rollback(ctx.get(),
                                        vname.c_str(),
                                        snap1.c_str()));

        EXPECT_EQ(-1,
                  ovs_snapshot_remove(ctx.get(),
                                      vname.c_str(),
                                      snap2.c_str()));
        EXPECT_EQ(ENOENT,
                  errno);

        EXPECT_EQ(0,
                  ovs_snapshot_remove(ctx.get(),
                                      vname.c_str(),
                                      snap1.c_str()));
    }

    using StressExtraFun = std::function<void(const bc::seconds&,
                                              const std::atomic<bool>&,
                                              const std::string& vname)>;

    void
    test_stress(const EnableHa enable_ha,
                uint16_t port = FileSystemTestSetup::local_edge_port(),
                StressExtraFun extra_fun = [](const bc::seconds&,
                                              const std::atomic<bool>&,
                                              const std::string&)
                {})
    {
        const std::string vname("volume");
        const size_t vsize = 1ULL << 20;

        make_volume(vname,
                    vsize,
                    port,
                    enable_ha);

        const size_t nreaders = yt::System::get_env_with_default("EDGE_STRESS_READERS",
                                                                 2ULL);
        const size_t qdepth = yt::System::get_env_with_default("EDGE_STRESS_QDEPTH",
                                                               1024ULL);
        const size_t duration_secs = yt::System::get_env_with_default("EDGE_STRESS_DURATION_SECS",
                                                                      30ULL);
        const size_t bufsize = yt::System::get_env_with_default("EDGE_STRESS_BUF_SIZE",
                                                                4096ULL);

        std::atomic<bool> stop(false);

        auto reader([&]() -> size_t
                    {
                        CtxPtr ctx(open_volume(vname,
                                               port,
                                               O_RDWR,
                                               enable_ha,
                                               qdepth));

                        std::vector<uint8_t> buf(bufsize);

                        using AiocbPtr = std::unique_ptr<ovs_aiocb>;
                        using AiocbQueue = std::deque<AiocbPtr>;

                        AiocbQueue free_queue;

                        for (size_t i = 0; i < qdepth; ++i)
                        {
                            AiocbPtr aiocb(std::make_unique<ovs_aiocb>());
                            aiocb->aio_buf = buf.data();
                            aiocb->aio_nbytes = buf.size();
                            aiocb->aio_offset = 0;

                            free_queue.emplace_back(std::move(aiocb));
                        }

                        AiocbQueue pending_queue;
                        size_t ops = 0;

                        while (true)
                        {
                            if (stop or pending_queue.size() == qdepth)
                            {
                                AiocbPtr aiocb;

                                if (not pending_queue.empty())
                                {
                                    aiocb = std::move(pending_queue.front());
                                    pending_queue.pop_front();

                                    EXPECT_EQ(0,
                                              ovs_aio_suspend(ctx.get(),
                                                              aiocb.get(),
                                                              nullptr));
                                    EXPECT_EQ(aiocb->aio_nbytes,
                                              ovs_aio_return(ctx.get(),
                                                             aiocb.get()));
                                    EXPECT_EQ(0,
                                              ovs_aio_finish(ctx.get(),
                                                             aiocb.get()));

                                    ops++;
                                }

                                if (stop)
                                {
                                    if (pending_queue.empty())
                                    {
                                        break;
                                    }
                                }
                                else
                                {
                                    EXPECT_TRUE(aiocb != nullptr);
                                    free_queue.emplace_back(std::move(aiocb));
                                }
                            }
                            else
                            {
                                EXPECT_FALSE(free_queue.empty());
                                AiocbPtr aiocb = std::move(free_queue.front());
                                free_queue.pop_front();

                                EXPECT_EQ(0,
                                          ovs_aio_read(ctx.get(),
                                                       aiocb.get()));
                                pending_queue.emplace_back(std::move(aiocb));
                            }
                        }

                        return ops;
                    });

        const bc::seconds duration(duration_secs);

        // this fails to compile:
        //
        // std::future<void> extra_future(std::async(std::launch::async,
        //                                           std::move(extra_fun),
        //                                           duration,
        //                                           stop,
        //                                           vname));
        //
        // as somewhere along the way of (perfect) forwarding the const std::atomic<bool>& is
        // converted into a std::atomic<bool> which fails due to copy construction.
        // If you, esteemed reader, know how to fix this please let me know / fix it, but
        // for now the ugly workaround below is used:
        std::future<void> extra_future(std::async(std::launch::async,
                                                  [&]
                                                  {
                                                      extra_fun(duration,
                                                                stop,
                                                                vname);
                                                  }));

        std::vector<std::future<size_t>> futures;
        futures.reserve(nreaders);

        yt::wall_timer t;

        for (size_t i = 0; i < nreaders; ++i)
        {
            futures.emplace_back(std::async(std::launch::async,
                                            reader));
        }

        boost::this_thread::sleep_for(duration);

        stop = true;
        size_t total = 0;

        for (auto& f : futures)
        {
            size_t ops = f.get();
            EXPECT_LT(0, ops);
            total += ops;

        }

        const double elapsed = t.elapsed();

        extra_future.wait();

        std::cout << nreaders << " workers doing " << bufsize <<
            " bytes sized reads @ qdepth " << qdepth << ": " <<
            total << " ops in " << elapsed << " seconds -> " <<
            total / elapsed << " IOPS" << std::endl;
    }

    void
    test_steal_remote_volume(bool stop_dtl)
    {
        set_use_fencing(true);

        RemoteMount mnt(make_remote_mount());

        const std::string vname("volume");
        const size_t vsize = 1ULL << 20;

        make_volume(vname,
                    vsize,
                    FileSystemTestSetup::remote_edge_port());

        CtxPtr ctx(open_volume(vname,
                               FileSystemTestSetup::local_edge_port(),
                               O_RDWR,
                               EnableHa::F));

        if (stop_dtl)
        {
            stop_failovercache_for_remote_node();
        }

        const std::string snap("snapshot-to-get-the-dtl-in-sync");
        EXPECT_EQ(0,
                  ovs_snapshot_create(ctx.get(),
                                      vname.c_str(),
                                      snap.c_str(),
                                      30));
        EXPECT_EQ(1,
                  ovs_snapshot_is_synced(ctx.get(),
                                         vname.c_str(),
                                         snap.c_str()));

        const std::string
            pattern("Remote volumes can only be stolen if the DTL is in sync!");
        ASSERT_EQ(pattern.size(),
                  ovs_write(ctx.get(),
                            reinterpret_cast<const uint8_t*>(pattern.data()),
                            pattern.size(),
                            0));
        ASSERT_EQ(0,
                  ovs_flush(ctx.get()));

        mnt.umount();

        std::vector<char> rbuf(pattern.size());
        const ssize_t ret = ovs_read(ctx.get(),
                                     reinterpret_cast<uint8_t*>(rbuf.data()),
                                     rbuf.size(),
                                     0);

        if (stop_dtl)
        {
            ASSERT_GT(0, ret);
        }
        else
        {
            ASSERT_EQ(ret, rbuf.size());
            ASSERT_EQ(pattern,
                      std::string(rbuf.data(),
                                  rbuf.size()));
        }
    }

    void
    test_high_availability(EnableHa enable_ha,
                           bool mark_offline,
                           bool fencing)
    {
        if (fencing)
        {
            set_use_fencing(true);
        }

        CtxPtr ctx;
        const std::string pattern("openvstorage1");
        const uint64_t volume_size = 1 << 20;

        {
            RemoteMount mnt(make_remote_mount());

            const std::string vname("volume");

            make_volume(vname,
                        volume_size,
                        remote_edge_port(),
                        enable_ha);

            ctx = open_volume(vname,
                              remote_edge_port(),
                              O_RDWR,
                              enable_ha,
                              64);

            auto wbuf = std::make_unique<uint8_t[]>(pattern.length());
            ASSERT_TRUE(wbuf != nullptr);

            memcpy(wbuf.get(),
                   pattern.c_str(),
                   pattern.length());

            EXPECT_EQ(pattern.length(),
                      ovs_write(ctx.get(),
                                wbuf.get(),
                                pattern.length(),
                                1024));

            EXPECT_EQ(0, ovs_flush(ctx.get()));

            auto rbuf = std::make_unique<uint8_t[]>(pattern.length());
            ASSERT_TRUE(rbuf != nullptr);

            EXPECT_EQ(pattern.length(),
                      ovs_read(ctx.get(),
                               rbuf.get(),
                               pattern.length(),
                               1024));

            EXPECT_TRUE(memcmp(rbuf.get(),
                               pattern.c_str(),
                               pattern.length()) == 0);

            const int64_t timeout = 10;
            const std::string snap1("snap1");

            EXPECT_EQ(-1,
                      ovs_snapshot_create(ctx.get(),
                                          (vname + "X").c_str(),
                                          snap1.c_str(),
                                          timeout));
            EXPECT_EQ(ENOENT,
                      errno);

            EXPECT_EQ(0,
                      ovs_snapshot_create(ctx.get(),
                                          vname.c_str(),
                                          snap1.c_str(),
                                          timeout));

            const std::string snap2("snap2");

            EXPECT_EQ(0,
                      ovs_snapshot_create(ctx.get(),
                                          vname.c_str(),
                                          snap2.c_str(),
                                          0));

            EXPECT_EQ(1,
                      ovs_snapshot_is_synced(ctx.get(),
                                             vname.c_str(),
                                             snap2.c_str()));
        }

        if (mark_offline)
        {
            std::shared_ptr<vfs::ClusterRegistry> reg(cluster_registry(fs_->object_router()));
            reg->set_node_state(remote_node_id(),
                                vfs::ClusterNodeStatus::State::Offline);
        }

        auto rbuf_local = std::make_unique<uint8_t[]>(pattern.length());
        ASSERT_TRUE(rbuf_local != nullptr);

        if (enable_ha == EnableHa::T)
        {
            EXPECT_EQ(pattern.length(),
                      ovs_read(ctx.get(),
                               rbuf_local.get(),
                               pattern.length(),
                               1024));

            EXPECT_TRUE(memcmp(rbuf_local.get(),
                               pattern.c_str(),
                               pattern.length()) == 0);

            EXPECT_EQ(0,
                      ovs_create_volume(ctx.get(),
                                        "local_volume",
                                        volume_size));
        }
        else
        {
            EXPECT_EQ(-1,
                      ovs_read(ctx.get(),
                               rbuf_local.get(),
                               pattern.length(),
                               1024)) << "errno: " << errno;
            EXPECT_EQ(ESHUTDOWN, errno);

            EXPECT_EQ(-1,
                      ovs_create_volume(ctx.get(),
                                        "local_volume",
                                        volume_size));
            EXPECT_EQ(ESHUTDOWN, errno);
        }
    }

    std::unique_ptr<NetworkXioInterface> net_xio_server_;
    boost::thread net_xio_thread_;
    vfs::PythonClient client_;
};

TEST_F(NetworkServerTest, uri)
{
    EXPECT_EQ(network_server_uri(local_node_id()),
              net_xio_server_->uri());
}

TEST_F(NetworkServerTest, create_destroy_context)
{
    uint64_t volume_size = 1ULL << 30;
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
    EXPECT_EQ(ENOENT, errno);
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
    uint64_t volume_size = 1ULL << 30;
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
    EXPECT_EQ(0, ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, open_same_volume_twice)
{
    uint64_t volume_size = 1ULL << 30;
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
    ASSERT_EQ(0,
              ovs_ctx_init(ctx1,
                           "volume",
                           O_RDWR));
    ASSERT_EQ(0,
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
    uint64_t volume_size = 1ULL << 30;
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
    ASSERT_EQ(0,ovs_ctx_init(ctx,
                             "volume",
                             O_RDWR));

    const size_t bufsize(yt::System::get_env_with_default("EDGE_TEST_BUFSIZE",
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

	ASSERT_EQ(0,
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

	ASSERT_EQ(0,
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

    uint64_t volume_size = 1ULL << 30;
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
    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));
    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));

    ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    ASSERT_EQ(0,
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
    uint64_t volume_size = 1ULL << 30;
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

TEST_F(NetworkServerTest, list_volumes)
{
    uint64_t volume_size = 1ULL << 30;
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

    uint64_t volume_size = 1ULL << 30;
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

    ASSERT_EQ(0,
              ovs_ctx_init(ctx1, "volume1", O_RDWR));

    ASSERT_EQ(0,
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
    uint64_t volume_size = 1ULL << 30;
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
    ASSERT_EQ(0,
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

TEST_F(NetworkServerTest, create_rollback_list_remove_snapshot_local)
{
    test_snapshot_ops(false);
}

TEST_F(NetworkServerTest, create_rollback_list_remove_snapshot_remote)
{
    RemoteMount mnt(make_remote_mount());
    test_snapshot_ops(true);
}

// reproduces https://github.com/openvstorage/volumedriver/issues/188
TEST_F(NetworkServerTest, list_snapshots)
{
    const std::string vname("volume");
    const size_t vsize = 1ULL << 20;

    CtxPtr ctx(make_volume(vname,
                           vsize,
                           FileSystemTestSetup::local_edge_port(),
                           EnableHa::F));

    std::set<std::string> snaps = { "snap1", "snap2shot", "snap3" };

    for (const auto& s : snaps)
    {
        EXPECT_EQ(0,
                  ovs_snapshot_create(ctx.get(),
                                      vname.c_str(),
                                      s.c_str(),
                                      0));
    }

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

    std::vector<std::string> vec;
    vec.reserve(snaps.size());

    int err = 0;
    uint64_t ssize = snaps.size();
    ctx_iface.list_snapshots(vec,
                             vname.c_str(),
                             &ssize,
                             &err);
    ASSERT_EQ(0, err);
    EXPECT_EQ(vec.size(), snaps.size());

    for (const auto& s : vec)
    {
        EXPECT_EQ(1,
                  snaps.erase(s)) << s << " is not a known snapshot";
    }

    EXPECT_TRUE(snaps.empty());
}

TEST_F(NetworkServerTest, connect_to_nonexistent_port)
{
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         8));
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx == nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, stress)
{
    test_stress(EnableHa::F);
}

TEST_F(NetworkServerTest, stress_ha_enabled)
{
    test_stress(EnableHa::T);
}

TEST_F(NetworkServerTest, create_truncate_volume)
{
    uint64_t volume_size = 1ULL << 20;
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
    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

    struct stat st;
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    uint64_t new_volume_size = 1ULL << 30;
    EXPECT_EQ(ovs_truncate_volume(ctx,
                                  "volume",
                                  new_volume_size),
              0);
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

TEST_F(NetworkServerTest, truncate_volume)
{
    uint64_t volume_size = 1ULL << 20;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));

    EXPECT_EQ(ovs_create_volume(nullptr,
                                "nullctx_volume",
                                volume_size),
              -1);

    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_create_volume(ctx,
                                "volume",
                                volume_size),
              0);

    uint64_t new_volume_size = 1ULL << 30;
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

TEST_F(NetworkServerTest, fail_to_truncate_volume)
{
    uint64_t volume_size = 1ULL << 20;
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

    uint64_t new_volume_size = 1ULL << 30;
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

TEST_F(NetworkServerTest, list_open_connections)
{
    uint64_t volume_size = 1ULL << 20;
    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    ASSERT_TRUE(ctx_attr != nullptr);
    EXPECT_EQ(0,
              ovs_ctx_attr_set_transport(ctx_attr,
                                         FileSystemTestSetup::edge_transport().c_str(),
                                         FileSystemTestSetup::address().c_str(),
                                         FileSystemTestSetup::local_edge_port()));

    const std::string vname("volume");
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    ASSERT_TRUE(ctx != nullptr);
    EXPECT_EQ(ovs_create_volume(ctx,
                                vname.c_str(),
                                volume_size),
              0);

    const std::vector<vfs::ClientInfo> m(client_.list_client_connections(local_node_id()));
    EXPECT_EQ(0, m.size());

    ASSERT_EQ(0,
              ovs_ctx_init(ctx,
                           vname.c_str(),
                           O_RDONLY));

    const std::vector<vfs::ClientInfo> l(client_.list_client_connections(local_node_id()));
    EXPECT_EQ(1, l.size());

    const boost::optional<vfs::ObjectId> oid(find_object(vfs::FrontendPath(make_volume_name("/" + vname))));
    const vfs::ClientInfo& info = l[0];

    EXPECT_EQ(oid,
              info.object_id);
    EXPECT_FALSE(info.ip.empty());
    EXPECT_NE(0,
              info.port);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));

    const std::vector<vfs::ClientInfo> k(client_.list_client_connections(local_node_id()));
    EXPECT_EQ(0, k.size());
}

TEST_F(NetworkServerTest, high_availability_fail_remote_mark_offline)
{
    test_high_availability(EnableHa::T,
                           true,
                           false);
}

TEST_F(NetworkServerTest, high_availability_fail_remote_and_fail)
{
    test_high_availability(EnableHa::F,
                           true,
                           false);
}

TEST_F(NetworkServerTest, high_availability_fail_remote_automated)
{
    test_high_availability(EnableHa::T,
                           false,
                           true);
}

TEST_F(NetworkServerTest, get_volume_uri)
{
    const std::string vname("volume");
    const size_t vsize = 1ULL << 20;
    CtxPtr ctx(make_volume(vname,
                           vsize,
                           FileSystemTestSetup::local_edge_port(),
                           EnableHa::F));

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

    std::string uri;
    ctx_iface.get_volume_uri(vname.c_str(),
                             uri);

    EXPECT_EQ(network_server_uri(local_node_id()),
              yt::Uri(uri));
}

TEST_F(NetworkServerTest, get_volume_uri_errors)
{
    CtxPtr ctx;
    const std::string vname("volume");

    {
        RemoteMount mnt(make_remote_mount());
        const size_t vsize = 1ULL << 20;

        ctx = make_volume(vname,
                          vsize,
                          FileSystemTestSetup::remote_edge_port(),
                          EnableHa::F);
    }

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

    std::string uri;
    EXPECT_NE(0, ctx_iface.get_volume_uri(vname.c_str(),
                                          uri));
}

TEST_F(NetworkServerTest, get_volume_uri_stress)
{
    const std::string vname("volume");
    const size_t vsize = 1ULL << 20;
    const uint16_t port = FileSystemTestSetup::local_edge_port();

    make_volume(vname,
                vsize,
                port,
                EnableHa::F);

    const size_t nthreads = yt::System::get_env_with_default("EDGE_STRESS_READERS",
                                                             4ULL);
    const size_t duration_secs = yt::System::get_env_with_default("EDGE_STRESS_DURATION_SECS",
                                                                  10ULL);

    using Clock = bc::steady_clock;
    const Clock::time_point deadline = Clock::now() + bc::seconds(duration_secs);

    const std::string exp_uri(boost::lexical_cast<std::string>(network_server_uri(local_node_id())));

    auto fun([&]
             {
                 CtxPtr ctx(open_volume(vname,
                                        port,
                                        O_RDWR,
                                        EnableHa::F,
                                        1));

                 auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

                 while (Clock::now() < deadline)
                 {
                     std::string uri;
                     EXPECT_EQ(0,
                               ctx_iface.get_volume_uri(vname.c_str(),
                                                        uri));
                     EXPECT_EQ(exp_uri,
                               uri);
                 }
             });

    std::vector<boost::thread> threads;
    threads.reserve(nthreads);

    for (size_t i = 0; i < nthreads; ++i)
    {
        threads.emplace_back(fun);
    }

    for (auto& t : threads)
    {
        t.join();
    }
}

TEST_F(NetworkServerTest, redirect_uri)
{
    RemoteMount mnt(make_remote_mount());

    const std::string vname("some-volume");
    const size_t vsize = 1ULL << 20;

    make_volume(vname,
                vsize,
                FileSystemTestSetup::local_edge_port());

    CtxPtr ctx(open_volume(vname,
                           FileSystemTestSetup::remote_edge_port(),
                           O_RDWR,
                           EnableHa::T));

    auto& ha_ctx = dynamic_cast<libovs::NetworkHAContext&>(*ctx);

    using Clock = bc::steady_clock;
    const Clock::time_point end = Clock::now() + bc::seconds(60);

    while (Clock::now() < end)
    {
        if (yt::Uri(ha_ctx.current_uri()) == network_server_uri(local_node_id()))
        {
            break;
        }
        boost::this_thread::sleep_for(bc::milliseconds(250));
    }

    EXPECT_EQ(network_server_uri(local_node_id()),
              yt::Uri(ha_ctx.current_uri()));
}

TEST_F(NetworkServerTest, ha_stress)
{
    set_use_fencing(true);
    RemoteMount mnt(make_remote_mount());

    const int hard_kill = yt::System::get_env_with_default("EDGE_HA_STRESS_KILL_REMOTE",
                                                           1);

    test_stress(EnableHa::T,
                FileSystemTestSetup::remote_edge_port(),
                [&](const bc::seconds& runtime,
                    const std::atomic<bool>&,
                    const std::string&)
                {
                    const bc::seconds wait(20);
                    EXPECT_LT(wait,
                              runtime);

                    boost::this_thread::sleep_for(wait);

                    if (hard_kill)
                    {
                        ::kill(remote_pid_, SIGKILL);
                    }

                    mnt.umount(hard_kill);
                });
}

TEST_F(NetworkServerTest, list_uris)
{
    std::shared_ptr<ClusterRegistry> creg(fs_->object_router().cluster_registry());
    const ClusterNodeConfigs configs(creg->get_node_configs());
    ASSERT_LT(0, configs.size());

    CtxAttrPtr attrs(make_ctx_attr(1024,
                                   EnableHa::F,
                                   FileSystemTestSetup::local_edge_port()));
    CtxPtr ctx(ovs_ctx_new(attrs.get()));
    ASSERT_TRUE(ctx != nullptr);

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

    std::vector<std::string> uris;
    uris.reserve(configs.size());

    EXPECT_EQ(0,
              ctx_iface.list_cluster_node_uri(uris));
    EXPECT_EQ(configs.size(),
              uris.size());

    for (const auto& u : uris)
    {
        bool found = false;
        for (const auto& c : configs)
        {
            ASSERT_NE(boost::none,
                      c.network_server_uri);
            if (u == boost::lexical_cast<std::string>(*c.network_server_uri))
            {
                found = true;
            }
        }

        EXPECT_TRUE(found);
    }
}

TEST_F(NetworkServerTest, list_uris_filter)
{
    const size_t ghost_nodes = 3;
    const ClusterNodeConfigs
        configs(update_cluster_registry([&](ClusterNodeConfigs& cfgs)
                                        {
                                            ASSERT_FALSE(cfgs.empty());
                                            add_ghost_node_configs(ghost_nodes,
                                                                   cfgs);

                                            for (auto& cfg : cfgs)
                                            {
                                                ClusterNodeConfig::NodeDistanceMap map;
                                                uint32_t distance = 0;
                                                for (const auto& c : cfgs)
                                                {
                                                    const auto res(map.emplace(c.vrouter_id, distance++));
                                                    ASSERT_TRUE(res.second);
                                                }
                                                cfg.node_distance_map = std::move(map);
                                            }
                                        }));

    CtxAttrPtr attrs(make_ctx_attr(1024,
                                   EnableHa::F,
                                   FileSystemTestSetup::local_edge_port()));
    CtxPtr ctx(ovs_ctx_new(attrs.get()));
    ASSERT_TRUE(ctx != nullptr);

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

    for (size_t i = 0; i <= configs.size(); ++i)
    {
        set_max_neighbour_distance(i);

        std::vector<std::string> uris;
        uris.reserve(i);

        EXPECT_EQ(0,
                  ctx_iface.list_cluster_node_uri(uris));
        EXPECT_EQ(i,
                  uris.size());

        for (size_t j = 0; j < uris.size(); ++j)
        {
            ASSERT_NE(boost::none,
                      configs[j].network_server_uri);
            EXPECT_EQ(boost::lexical_cast<std::string>(*configs[j].network_server_uri),
                      uris[j]);
        }
    }
}

TEST_F(NetworkServerTest, list_uris_rand_order)
{
    const size_t ghost_nodes = 23;
    std::set<std::string> set;
    const ClusterNodeConfigs
        configs(update_cluster_registry([&](ClusterNodeConfigs& cfgs)
        {
            ASSERT_EQ(2, cfgs.size());
            add_ghost_node_configs(ghost_nodes,
                                   cfgs);

            ClusterNodeConfig::NodeDistanceMap map;
            for (const auto& c : cfgs)
            {
                uint32_t d;
                if (c == cfgs.front())
                {
                    d = 0;
                }
                else if (c == cfgs.back())
                {
                    d = 10;
                }
                else
                {
                    d = 5;
                    set.emplace(boost::lexical_cast<std::string>(*c.network_server_uri));
                }

                map.emplace(c.vrouter_id, d);
            }

            cfgs.front().node_distance_map = std::move(map);

            for (size_t i = 1; i < cfgs.size(); ++i)
            {
                cfgs[i].node_distance_map = ClusterNodeConfig::NodeDistanceMap();
            }
        }));

    ASSERT_EQ(ghost_nodes,
              set.size());

    CtxAttrPtr attrs(make_ctx_attr(1024,
                                   EnableHa::F,
                                   FileSystemTestSetup::local_edge_port()));
    CtxPtr ctx(ovs_ctx_new(attrs.get()));
    ASSERT_TRUE(ctx != nullptr);

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);
    std::vector<std::string> uris;
    uris.reserve(configs.size());

    EXPECT_EQ(0,
              ctx_iface.list_cluster_node_uri(uris));
    EXPECT_EQ(configs.size(),
              uris.size());

    for (const auto& u : uris)
    {
        if (u == uris.front())
        {
            EXPECT_EQ(boost::lexical_cast<std::string>(*configs.front().network_server_uri),
                      u);
        }
        else if (u == uris.back())
        {
            EXPECT_EQ(boost::lexical_cast<std::string>(*configs.back().network_server_uri),
                      u);
        }
        else
        {
            EXPECT_EQ(1, set.erase(u)) << u;
        }
    }

    EXPECT_TRUE(set.empty());
}

TEST_F(NetworkServerTest, steal_remote_volume_if_dtl_is_in_sync)
{
    test_steal_remote_volume(false);
}

TEST_F(NetworkServerTest, dont_steal_remote_volume_if_dtl_is_not_in_sync)
{
    test_steal_remote_volume(true);
}

TEST_F(NetworkServerTest, get_cluster_multiplier)
{
    const std::string vname("volume");
    const size_t vsize = 1ULL << 20;

    CtxPtr ctx(make_volume(vname,
                           vsize,
                           FileSystemTestSetup::local_edge_port(),
                           EnableHa::F));

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

    uint32_t cluster_multiplier;
    ctx_iface.get_cluster_multiplier(vname.c_str(),
                                     &cluster_multiplier);

    EXPECT_EQ(vd::VolumeConfig::default_cluster_multiplier(),
              cluster_multiplier);
}

TEST_F(NetworkServerTest, get_clone_namespace_map)
{
    using CloneNamespaceMap = std::map<uint8_t, std::string>;

    const std::string vname("volume");
    const size_t vsize = 1ULL << 20;

    CtxPtr ctx(make_volume(vname,
                           vsize,
                           FileSystemTestSetup::local_edge_port(),
                           EnableHa::F));

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

    CloneNamespaceMap cn;
    ctx_iface.get_clone_namespace_map(vname.c_str(),
                                      cn);

    ASSERT_EQ(1UL, cn.size());

    auto conn(cm_->getConnection());
    vd::VolumeConfig cfg;
    {
        LOCKVD();
        const FrontendPath vname(make_volume_name("/volume"));
        auto object_id = find_object(vname);
        ASSERT_NE(boost::none,
                  object_id);
        cfg = api::getVolumeConfig(vd::VolumeId(*object_id));
    }
    EXPECT_TRUE(conn->namespaceExists(cfg.getNS()));
    EXPECT_EQ(cfg.getNS().str(), cn[0]);
}

TEST_F(NetworkServerTest, get_page)
{
    const std::string vname("volume");
    const size_t vsize = 1ULL << 20;

    CtxPtr ctx(make_volume(vname,
                           vsize,
                           FileSystemTestSetup::local_edge_port(),
                           EnableHa::F));

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

    libovsvolumedriver::ClusterLocationPage clp;
    ctx_iface.get_page(vname.c_str(),
                       libovsvolumedriver::ClusterAddress(0),
                       clp);

    ASSERT_EQ(vd::CachePage::capacity(), clp.size());
    for (const auto& e: clp)
    {
        EXPECT_TRUE(e == libovsvolumedriver::ClusterLocation(0));
    }

    std::string pattern("openvstorage1");
    auto wbuf = std::make_unique<uint8_t[]>(pattern.length());
    ASSERT_TRUE(wbuf != nullptr);

    memcpy(wbuf.get(),
           pattern.c_str(),
           pattern.length());

    ASSERT_EQ(0,
              ovs_ctx_init(ctx.get(),
                           vname.c_str(),
                           O_RDWR));

    EXPECT_EQ(pattern.length(),
              ovs_write(ctx.get(),
                        wbuf.get(),
                        pattern.length(),
                        0));

    EXPECT_EQ(0, ovs_flush(ctx.get()));

    wbuf.reset();

    auto rbuf = std::make_unique<uint8_t[]>(pattern.length());
    ASSERT_TRUE(rbuf != nullptr);

    EXPECT_EQ(pattern.length(),
              ovs_read(ctx.get(),
                       rbuf.get(),
                       pattern.length(),
                       0));

    EXPECT_TRUE(memcmp(rbuf.get(),
                       pattern.c_str(),
                       pattern.length()) == 0);

    rbuf.reset();

    ctx_iface.get_page(vname.c_str(),
                       libovsvolumedriver::ClusterAddress(0),
                       clp);

    ASSERT_EQ(vd::CachePage::capacity(), clp.size());
    for (const auto& e: clp)
    {
        EXPECT_EQ(libovs::ClusterLocation(0),
                  e);
    }

    // Trigger uncork.
    ASSERT_EQ(0,
              ovs_snapshot_create(ctx.get(),
                                  vname.c_str(),
                                  "snapshot-1",
                                  60));

    // There's a race between
    // * marking the tlog as on the backend + uncork
    // * checking whether the snapshot is on the backend
    // . Creating a second snapshot closes that.
    ASSERT_EQ(0,
              ovs_snapshot_create(ctx.get(),
                                  vname.c_str(),
                                  "snapshot-2",
                                  60));

    clp.clear();
    ctx_iface.get_page(vname.c_str(),
                       libovs::ClusterAddress(0),
                       clp);

    ASSERT_EQ(vd::CachePage::capacity(), clp.size());
    for (const auto& e: clp)
    {
        if (e == clp[0])
        {
            EXPECT_EQ(libovs::ClusterLocation(1),
                      e);
        }
        else
        {
            EXPECT_EQ(libovs::ClusterLocation(0),
                      e);
        }
    }

    vd::WeakVolumePtr v;

    {
        const FrontendPath fpath("/volume-flat.vmdk");
        auto maybe_id(find_object(fpath));
        LOCKVD();
        v = api::getVolumePointer(vd::VolumeId(*maybe_id));
    }

    const vd::ClusterAddress ca(0);
    std::vector<vd::ClusterLocation> cloc(api::GetPage(v, ca));

    ASSERT_EQ(vd::CachePage::capacity(), cloc.size());
    for (const auto& e: cloc)
    {
        if (e == cloc[0])
        {
            EXPECT_TRUE(e == vd::ClusterLocation(1));
        }
        else
        {
            EXPECT_TRUE(e == vd::ClusterLocation(0));
        }
    }
}

TEST_F(NetworkServerTest, DISABLED_remote_going_away_during_ctrl_request)
{
    const int hard_kill = yt::System::get_env_with_default("EDGE_HA_STRESS_KILL_REMOTE",
                                                           1);

    RemoteMount mnt(make_remote_mount());

    std::atomic<bool> stop(false);
    CtxAttrPtr attrs(make_ctx_attr(1024,
                                   EnableHa::T,
                                   FileSystemTestSetup::remote_edge_port()));
    CtxPtr ctx(ovs_ctx_new(attrs.get()));
    ASSERT_TRUE(ctx != nullptr);

    auto& ctx_iface = dynamic_cast<ovs_context_t&>(*ctx);

    std::future<void>
        f(std::async(std::launch::async,
                     [&]
                     {
                         do
                         {
                             std::vector<std::string> uris;
                             ctx_iface.list_cluster_node_uri(uris);
                         }
                         while (not stop);
                     }));

    boost::this_thread::sleep_for(bc::seconds(1));

    if (hard_kill)
    {
        ::kill(remote_pid_, SIGKILL);
    }

    mnt.umount(hard_kill);
    stop = true;

    f.wait();
}

TEST_F(NetworkServerTest, write_beyond_volume_boundaries)
{
    uint64_t volume_size = 1ULL << 20;
    off_t offset = 1ULL << 23;
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
    ASSERT_EQ(0,
              ovs_ctx_init(ctx,
                           "volume",
                           O_RDWR));

    std::string pattern("openvstorage1");
    auto wbuf = std::make_unique<uint8_t[]>(pattern.length());
    ASSERT_TRUE(wbuf != nullptr);

    memcpy(wbuf.get(),
           pattern.c_str(),
           pattern.length());

    EXPECT_EQ(-1,
              ovs_write(ctx,
                        wbuf.get(),
                        pattern.length(),
                        offset));

    EXPECT_EQ(EFBIG, errno);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, remote_write_beyond_volume_boundaries)
{
    RemoteMount mnt(make_remote_mount());

    const std::string vname("volume");
    off_t offset = 2147483648;

    make_volume(vname,
                1ULL << 20,
                edge_port(true));

    CtxPtr ctx(open_volume(vname,
                           edge_port(true),
                           O_RDWR,
                           EnableHa::F));
    ASSERT_TRUE(ctx != nullptr);

    std::string pattern("openvstorage1");
    auto wbuf = std::make_unique<uint8_t[]>(pattern.length());
    ASSERT_TRUE(wbuf != nullptr);

    memcpy(wbuf.get(),
           pattern.c_str(),
           pattern.length());

    EXPECT_EQ(-1,
              ovs_write(ctx.get(),
                        wbuf.get(),
                        pattern.length(),
                        offset));

    EXPECT_EQ(EFBIG, errno);
}

TEST_F(NetworkServerTest, fail_shrink_volume)
{
    uint64_t volume_size = 1ULL << 30;
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
    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

    struct stat st;
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    uint64_t new_volume_size = 1ULL << 20;
    EXPECT_EQ(-1, ovs_truncate_volume(ctx,
                                     "volume",
                                     new_volume_size));
    EXPECT_EQ(EPERM, errno);
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, remote_fail_shrink_volume)
{
    RemoteMount mnt(make_remote_mount());

    const std::string vname("volume");
    uint64_t volume_size = 1ULL << 30;
    make_volume(vname,
                volume_size,
                edge_port(true));

    CtxPtr ctx(open_volume(vname,
                           edge_port(true),
                           O_RDWR,
                           EnableHa::F));
    ASSERT_TRUE(ctx != nullptr);

    struct stat st;
    EXPECT_EQ(0,
              ovs_stat(ctx.get(), &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    uint64_t new_volume_size = 1ULL << 20;
    EXPECT_EQ(-1, ovs_truncate_volume(ctx.get(),
                                     "volume",
                                     new_volume_size));
    EXPECT_EQ(EPERM, errno);
    EXPECT_EQ(0,
              ovs_stat(ctx.get(), &st));
    EXPECT_EQ(st.st_size,
              volume_size);
}

TEST_F(NetworkServerTest, fail_grow_volume_beyond_limit)
{
    uint64_t volume_size = 1ULL << 30;
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
    ASSERT_EQ(0,
              ovs_ctx_init(ctx, "volume", O_RDWR));

    struct stat st;
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    uint64_t new_volume_size = UINT64_MAX;
    EXPECT_EQ(-1, ovs_truncate_volume(ctx,
                                     "volume",
                                     new_volume_size));
    EXPECT_EQ(EFBIG, errno);
    EXPECT_EQ(0,
              ovs_stat(ctx, &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    EXPECT_EQ(0,
              ovs_ctx_destroy(ctx));
    EXPECT_EQ(0,
              ovs_ctx_attr_destroy(ctx_attr));
}

TEST_F(NetworkServerTest, remote_fail_grow_volume_beyond_limit)
{
    RemoteMount mnt(make_remote_mount());

    const std::string vname("volume");
    uint64_t volume_size = 1ULL << 20;
    make_volume(vname,
                volume_size,
                edge_port(true));

    CtxPtr ctx(open_volume(vname,
                           edge_port(true),
                           O_RDWR,
                           EnableHa::F));
    ASSERT_TRUE(ctx != nullptr);

    struct stat st;
    EXPECT_EQ(0,
              ovs_stat(ctx.get(), &st));
    EXPECT_EQ(st.st_size,
              volume_size);

    uint64_t new_volume_size = UINT64_MAX;
    EXPECT_EQ(-1, ovs_truncate_volume(ctx.get(),
                                     "volume",
                                     new_volume_size));
    EXPECT_EQ(EFBIG, errno);
    EXPECT_EQ(0,
              ovs_stat(ctx.get(), &st));
    EXPECT_EQ(st.st_size,
              volume_size);
}

TEST_F(NetworkServerTest, unmount_while_doing_io)
{
    RemoteMount mnt(make_remote_mount());

    const std::string vname("volume");
    uint64_t volume_size = 1ULL << 20;
    make_volume(vname,
                volume_size,
                edge_port(true));

    CtxPtr ctx(open_volume(vname,
                           edge_port(true),
                           O_RDWR,
                           EnableHa::F));
    ASSERT_TRUE(ctx != nullptr);

    auto io_fun = [&]() {
        while (true)
        {
            std::vector<uint8_t> buf(4096);

            ovs_aiocb aiocb;
            aiocb.aio_buf = buf.data();
            aiocb.aio_nbytes = buf.size();
            aiocb.aio_offset = 0;

            EXPECT_EQ(0,ovs_aio_read(ctx.get(), &aiocb));
            ovs_aio_suspend(ctx.get(), &aiocb, nullptr);
            auto ret = ovs_aio_return(ctx.get(), &aiocb);
            if (ret < 0 && errno == ESHUTDOWN)
            {
                ovs_aio_finish(ctx.get(), &aiocb);
                break;
            }
            ovs_aio_finish(ctx.get(), &aiocb);
        }
    };

    auto io_thread = std::thread(io_fun);

    const bc::seconds duration(5);
    boost::this_thread::sleep_for(duration);

    mnt.umount();

    io_thread.join();
}

TEST_F(NetworkServerTest, unmount_while_having_many_open_connections)
{
    const size_t nconns = yt::System::get_env_with_default("EDGE_NCONS",
                                                           12ULL);

    RemoteMount mnt(make_remote_mount());

    const std::string vname("volume");
    uint64_t volume_size = 1ULL << 20;
    make_volume(vname,
                volume_size,
                edge_port(true));

    std::vector<CtxPtr> connections;

    for (size_t i = 0; i < nconns; i++)
    {
        CtxPtr ctx(open_volume(vname,
                               edge_port(true),
                               O_RDWR,
                               EnableHa::F));
        ASSERT_TRUE(ctx != nullptr);
        connections.push_back(std::move(ctx));
    }

    const bc::seconds duration(1);
    boost::this_thread::sleep_for(duration);
    mnt.umount();
    boost::this_thread::sleep_for(duration);
}

} //namespace
