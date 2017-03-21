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

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/python/extract.hpp>

#include <xmlrpc++0.7/src/XmlRpcClient.h>

#include <youtils/Catchers.h>
#include <youtils/IOException.h>
#include <youtils/InitializedParam.h>
#include <youtils/FileDescriptor.h>
#include <youtils/FileUtils.h>
#include <youtils/ScopeExit.h>
#include <youtils/Uri.h>

#include <backend/BackendConfig.h>

#include <volumedriver/metadata-server/Manager.h>
#include <volumedriver/SnapshotPersistor.h>
#include <volumedriver/Volume.h>
#include <volumedriver/ScrubberAdapter.h>
#include <volumedriver/ScrubReply.h>
#include <volumedriver/ScrubWork.h>
#include <volumedriver/test/MDSTestSetup.h>

#include "../CloneFileFlags.h"
#include "../LockedPythonClient.h"
#include "../PythonClient.h"
#include "../ObjectRouter.h"
#include "../Registry.h"
#include "../ScrubManager.h"
#include "../XMLRPC.h"
#include "../XMLRPCKeys.h"
#include "../XMLRPCStructs.h"

namespace volumedriverfstest
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace yt = youtils;

using namespace volumedriverfs;
using namespace std::literals::string_literals;

#define LOCKVD()                                                        \
    std::lock_guard<fungi::Mutex> vdg__(api::getManagementMutex())

namespace
{

struct SomeRedirects
    : public ::XmlRpc::XmlRpcServerMethod
{
    SomeRedirects(unsigned num_redirects,
                  const std::string& name,
                  uint16_t redir_port)
        : ::XmlRpc::XmlRpcServerMethod(name, nullptr)
        , max_redirects(num_redirects)
        , redirects(0)
        , port(redir_port)
    {}

    virtual ~SomeRedirects()
    {}

    const unsigned max_redirects;
    unsigned redirects;
    const uint16_t port;

    virtual void
    execute(::XmlRpc::XmlRpcValue& /* params */,
            ::XmlRpc::XmlRpcValue& result)
    {
        result.clear();

        if (redirects++ < max_redirects)
        {
            result[XMLRPCKeys::xmlrpc_redirect_host] =
                FileSystemTestSetup::address();
            result[XMLRPCKeys::xmlrpc_redirect_port] =
                boost::lexical_cast<std::string>(port);
        }
    }
};

}

class PythonClientTest
    : public FileSystemTestBase
{
protected:
    PythonClientTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("PythonClientTest")
                             .migrate_timeout_ms(5000)
                             .scrub_manager_interval_secs(1))
        , client_(vrouter_cluster_id(),
                  {{address(), local_config().xmlrpc_port}})
    {
        Py_Initialize();
    }

    virtual
    ~PythonClientTest()
    {
        Py_Finalize();
    }

    DECLARE_LOGGER("PythonClientTest");

    std::vector<std::string>
    list_methods()
    {
        return client_.list_methods();
    }

    void
    test_fallback()
    {
        const std::string rev(client_.server_revision());
        XmlRpc::XmlRpcValue req;
        ASSERT_EQ(rev,
                  client_.fallback_<std::string>("frobnicate",
                                                 Revision::method_name(),
                                                 req,
                                                 boost::none,
                                                 [](XmlRpc::XmlRpcValue&) -> std::string
                      {
                          EXPECT_TRUE(false) <<
                              "this should not be reached, the fallback needs to be called instead";
                          return "oh no!";
                      },
                                                 [](XmlRpc::XmlRpcValue& rsp) -> std::string
                      {
                          return static_cast<std::string>(rsp);
                      }));
    }

    void
    check_snapshot(const std::string& vname,
                   const std::string& sname,
                   const std::string& meta = "")
    {
        XMLRPCSnapshotInfo snap_info(client_.info_snapshot(vname, sname));

        EXPECT_TRUE(sname == snap_info.snapshot_id);
        EXPECT_TRUE(meta == snap_info.metadata);
    }

    void
    test_redirects(unsigned max_redirects)
    {
        // we can reuse the remote xmlrpc port as we won't spawn another fs instance
        const uint16_t port = static_cast<uint16_t>(remote_config().xmlrpc_port);
        xmlrpc::Server srv(address(),
                           port);

        std::unique_ptr<::XmlRpc::XmlRpcServerMethod>
            method(new SomeRedirects(max_redirects,
                                     "snapshotDestroy",
                                     port));

        srv.addMethod(std::move(method));
        srv.start();

        auto on_exit(yt::make_scope_exit([&]
                                         {
                                             srv.stop();
                                             srv.join();
                                         }));

        PythonClient remoteclient(vrouter_cluster_id(),
                                  {{address(), port}});
        const ObjectId id(yt::UUID().str());
        const std::string snap("snapshot");

        if (max_redirects > client_.max_redirects)
        {
            EXPECT_THROW(remoteclient.delete_snapshot(id, snap),
                         clienterrors::MaxRedirectsExceededException);
        }
        else
        {
            EXPECT_NO_THROW(remoteclient.delete_snapshot(id, snap));
        }
    }

    bpy::tuple
    scrub_wrap(const std::string& work_str)
    {
        using namespace scrubbing;

        const ScrubWork work(work_str);
        const yt::Uri loc(configuration_.string());
        const ScrubReply reply(ScrubberAdapter::scrub(be::BackendConfig::makeBackendConfig(loc),
                                                      work,
                                                      yt::FileUtils::temp_path().string()));
        return bpy::make_tuple(work.id_.str(),
                               reply.str());
    }

    void
    check_foc_config(const std::string& vname,
                     const FailOverCacheConfigMode exp_mode,
                     const boost::optional<vd::FailOverCacheConfig>& exp_config)
    {
        EXPECT_EQ(exp_mode,
                  client_.get_failover_cache_config_mode(vname));
        EXPECT_EQ(exp_config,
                  client_.get_failover_cache_config(vname));
    }

    void
    check_foc_state(const std::string& vname,
                    const vd::VolumeFailOverState state)
    {
        const XMLRPCVolumeInfo info(client_.info_volume(vname));
        EXPECT_EQ(state,
                  boost::lexical_cast<vd::VolumeFailOverState>(info.failover_mode));
    }

    vd::FailOverCacheConfig
    check_initial_foc_config(const std::string& vname)
    {
        const vd::FailOverCacheConfig cfg(remote_config().failovercache_host,
                                          remote_config().failovercache_port,
                                          vd::FailOverCacheMode::Asynchronous);
        check_foc_config(vname,
                         FailOverCacheConfigMode::Automatic,
                         cfg);

        return cfg;
    }

    static void
    halt_volume(const ObjectId& oid)
    {
        LOCKVD();
        vd::SharedVolumePtr
            v(api::getVolumePointer(static_cast<const vd::VolumeId>(oid)));
        v->halt();
    }

    void
    simulate_fencing(const ObjectId& oid)
    {
        halt_volume(oid);

        std::shared_ptr<CachedObjectRegistry>
            oreg(fs_->object_router().object_registry());
        oreg->migrate(oid,
                      local_node_id(),
                      remote_node_id());

        ASSERT_EQ(remote_node_id(),
                  oreg->find_throw(oid,
                                   IgnoreCache::T)->node_id);

        PythonClient rclient(vrouter_cluster_id(),
                             {{ remote_config().xmlrpc_host,
                                remote_config().xmlrpc_port }});

        rclient.migrate(oid,
                        remote_node_id(),
                        true);
    }

    PythonClient client_;
};

// Oh yeah, without Py_Initialize (and its Py_Finalize counterpart) even things
// as simple as that won't work.
TEST_F(PythonClientTest, DISABLED_pylist)
{
    bpy::list l;
    EXPECT_EQ(0, bpy::len(l));

    const std::string s("foo");
    l.append(s);

    EXPECT_EQ(1, bpy::len(l));
    const std::string t = bpy::extract<std::string>(l[0]);
    EXPECT_EQ(s, t);
}

TEST_F(PythonClientTest, no_one_listening)
{
    // there should not be anyone listening on the remote XMLRPC port
    PythonClient c(vrouter_cluster_id(),
                   {{address(), remote_config().xmlrpc_port}});
    EXPECT_THROW(c.list_volumes(), clienterrors::ClusterNotReachableException);
}

TEST_F(PythonClientTest, wrong_cluster_id)
{
    // there should not be anyone listening on the remote XMLRPC port
    PythonClient c("non-existing-cluster-id",
                   {{address(),
                               local_config().xmlrpc_port}});
    EXPECT_THROW(c.list_volumes(),
                 clienterrors::PythonClientException);
}

TEST_F(PythonClientTest, list_volumes)
{
    const ObjectId fid(create_file(FrontendPath("/some-file"s),
                                   4096));

    EXPECT_TRUE(client_.list_volumes().empty());

    std::set<std::string> volumes;
    const uint32_t num_volumes = 2;

    for (uint32_t i = 0; i < num_volumes; ++i)
    {
        const FrontendPath
            vpath(make_volume_name("/vol-"s +
                                   boost::lexical_cast<std::string>(i)));
        const ObjectId vname(create_file(vpath, 10 << 20));
        const auto res(volumes.insert(vname));
        EXPECT_TRUE(res.second);
    }

    const std::vector<std::string> vs(client_.list_volumes());
    EXPECT_EQ(num_volumes,
              vs.size());

    for (const auto& v : vs)
    {
        EXPECT_TRUE(volumes.find(v) != volumes.end());
    }
}

TEST_F(PythonClientTest, list_volumes_by_path)
{
    const ObjectId fid(create_file(FrontendPath("/some-file"s),
                                   4096));

    EXPECT_TRUE(client_.list_volumes().empty());

    std::set<std::string> volumes;
    const uint32_t num_volumes = 2;

    for (uint32_t i = 0; i < num_volumes; ++i)
    {
        const FrontendPath
            vpath(make_volume_name("/vol-"s +
                                   boost::lexical_cast<std::string>(i)));
        const ObjectId vname(create_file(vpath, 10 << 20));
        const auto res(volumes.insert(vpath.string()));
        EXPECT_TRUE(res.second);
    }

    const std::vector<std::string> vs(client_.list_volumes_by_path());
    EXPECT_EQ(num_volumes,
              vs.size());

    for (const auto& v : vs)
    {
        EXPECT_TRUE(volumes.find(v) != volumes.end());
    }
}

TEST_F(PythonClientTest, list_volumes_by_node)
{
    mount_remote();
    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         umount_remote();
                                     }));

    const uint64_t vsize = 1ULL << 20;
    const FrontendPath fname(make_volume_name("/some-volume"));
    const ObjectId id(create_file(fname, vsize));

    EXPECT_TRUE(client_.list_volumes(remote_node_id().str()).empty());

    {
        const std::vector<std::string> vs(client_.list_volumes(local_node_id().str()));
        ASSERT_EQ(1,
                  vs.size());
        EXPECT_EQ(id.str(),
                  vs[0]);
    }
}

TEST_F(PythonClientTest, snapshot_excessive_metadata)
{
    const FrontendPath vpath(make_volume_name("/some-volume"));
    const std::string vname(create_file(vpath, 10 << 20));
    const std::string snapname("snapshot");

    std::vector<char> metav(vd::SnapshotPersistor::max_snapshot_metadata_size + 1,
                            'z');
    const std::string meta(metav.begin(), metav.end());

    EXPECT_THROW(client_.create_snapshot(vname, snapname, meta),
                 clienterrors::PythonClientException);

    EXPECT_TRUE(client_.list_snapshots(vname).empty());
}

TEST_F(PythonClientTest, volume_potential)
{
    uint64_t res = 0;
    ASSERT_NO_THROW(res = client_.volume_potential(local_node_id()) );
    EXPECT_LT(0U, res);
    EXPECT_EQ(res,
              fs_->object_router().local_volume_potential(boost::none,
                                                          boost::none,
                                                          boost::none));
}

TEST_F(PythonClientTest, snapshot_metadata)
{
    const FrontendPath vpath(make_volume_name("/some-volume"));
    const std::string vname(create_file(vpath, 10 << 20));
    const std::string meta("some metadata");
    const std::string snapname("snapshot");

    client_.create_snapshot(vname, snapname, meta);
    check_snapshot(vname, snapname, meta);
}

TEST_F(PythonClientTest, snapshot_management)
{
    const FrontendPath vpath(make_volume_name("/some-volume"));
    const std::string vname(create_file(vpath, 10 << 20));

    const uint32_t snap_num = 10;
    std::vector<std::string> modeled_snapshots;
    modeled_snapshots.reserve(snap_num);

    for (uint32_t i = 0; i < snap_num; i++)
    {
        std::string snapname;
        while(true)
        {
            try
            {
                snapname = client_.create_snapshot(vname);
                break;
            }
            catch(clienterrors::PreviousSnapshotNotOnBackendException&)
            {
                usleep(10000);
            }
        }


        modeled_snapshots.push_back(snapname);
        check_snapshot(vname, snapname);
        EXPECT_TRUE(modeled_snapshots == client_.list_snapshots(vname));
    }

    EXPECT_THROW(client_.delete_snapshot(vname, "non-existing-snaphot"),
                 clienterrors::SnapshotNotFoundException);

    for (uint32_t cnt = 0; cnt < 3; cnt++)
    {
        auto it = modeled_snapshots.begin() + 2;
        client_.delete_snapshot(vname, *it);
        modeled_snapshots.erase(it);
        EXPECT_TRUE(modeled_snapshots == client_.list_snapshots(vname));
    }
}

TEST_F(PythonClientTest, snapshot_recreation)
{
    const FrontendPath vpath(make_volume_name("/some-volume"));
    const std::string vname(create_file(vpath, 10 << 20));
    const std::string snap("snapshot");

    client_.create_snapshot(vname,
                            snap);

    const size_t max = 100;
    size_t count = 0;

    while (not client_.is_volume_synced_up_to_snapshot(vname,
                                                       snap))
    {
        ASSERT_GT(max, ++count) <<
            "failed to sync snapshot to backend after " << max << " attempts";
        boost::this_thread::sleep_for(boost::chrono::milliseconds(250));
    }

    EXPECT_THROW(client_.create_snapshot(vname,
                                         snap),
                 clienterrors::SnapshotNameAlreadyExistsException);
}

TEST_F(PythonClientTest, volume_queries)
{
    const FrontendPath vpath(make_volume_name("/testing_info"));
    const std::string vname(create_file(vpath, 10 << 20));

    EXPECT_EQ(1,
              client_.list_volumes().size());

    XMLRPCVolumeInfo vol_info;
    EXPECT_NO_THROW(vol_info = client_.info_volume(vname));

    EXPECT_EQ(vd::volumeFailoverStateToString(vd::VolumeFailOverState::OK_SYNC),
              vol_info.failover_mode);

    XMLRPCStatistics vol_statistics = client_.statistics_volume(vname);
}

namespace
{

struct PerfCounterExpectNothing
{
    template<typename T, typename U>
    void
    operator()(const vd::PerformanceCounter<T, U>& ctr) const
    {
        EXPECT_EQ(0U,
                  ctr.events());
        EXPECT_EQ(0U,
                  ctr.sum());
        EXPECT_EQ(0U,
                  ctr.sum_of_squares());
        EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
                  ctr.min());
        EXPECT_EQ(std::numeric_limits<uint64_t>::min(),
                  ctr.max());
        for (size_t i = 0; i < ctr.bucket_bounds().size(); ++i)
        {
            EXPECT_EQ(0, ctr.buckets()[i]);
        }
    };
};

struct PerfCounterExpectBucketsNotEmpty
{
    template<typename T, typename U>
    void
    operator()(const vd::PerformanceCounter<T, U>& ctr) const
    {
        bool empty = true;
        for (size_t i = 0; i < ctr.bucket_bounds().size(); ++i)
        {
            if (ctr.buckets()[i] != 0)
            {
                empty = false;
            }
        }

        EXPECT_FALSE(empty);
    }
};

}

TEST_F(PythonClientTest, performance_counters)
{
    mount_remote();

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         umount_remote();
                                     }));

    const FrontendPath vpath(make_volume_name("/testing_info"));
    const std::string vname(create_file(vpath, 10 << 20));

    const size_t csize = get_cluster_size(ObjectId(vname));

    auto expect_nothing([&](const XMLRPCStatistics& stats)
                        {
                            LOG_INFO("expect nothing: " << stats.str());
                            const PerfCounterExpectNothing expect_nothing_;
                            expect_nothing_(stats.performance_counters.write_request_size);
                            expect_nothing_(stats.performance_counters.read_request_size);
                            expect_nothing_(stats.performance_counters.sync_request_usecs);
                        });

    expect_nothing(client_.statistics_volume(vname,
                                             false));

    expect_nothing(client_.statistics_node(local_node_id().str(),
                                           false));

    expect_nothing(client_.statistics_node(remote_node_id().str(),
                                           false));

    const std::string pattern("The Good Son");

    write_to_file(vpath,
                  pattern,
                  csize,
                  0);

    auto expect_something([&](const XMLRPCStatistics& stats)
                          {
                              LOG_INFO("expect something: " << stats.str());
                              EXPECT_EQ(1U,
                                        stats.performance_counters.write_request_size.events());
                              EXPECT_EQ(csize,
                                        stats.performance_counters.write_request_size.sum());
                              EXPECT_EQ(csize * csize,
                                        stats.performance_counters.write_request_size.sum_of_squares());
                              EXPECT_GT(std::numeric_limits<uint64_t>::max(),
                                        stats.performance_counters.write_request_size.min());
                              EXPECT_EQ(4096U,
                                        stats.performance_counters.write_request_size.min());
                              EXPECT_LT(std::numeric_limits<uint64_t>::min(),
                                        stats.performance_counters.write_request_size.max());
                              EXPECT_EQ(csize,
                                        stats.performance_counters.write_request_size.max());

                              PerfCounterExpectBucketsNotEmpty()(stats.performance_counters.write_request_size);

                              const PerfCounterExpectNothing expect_nothing_;
                              expect_nothing_(stats.performance_counters.read_request_size);
                              expect_nothing_(stats.performance_counters.sync_request_usecs);
                          });

    expect_something(client_.statistics_volume(vname,
                                               false));

    expect_nothing(client_.statistics_node(remote_node_id().str(),
                                           false));

    expect_something(client_.statistics_node(local_node_id().str(),
                                             true));

    expect_nothing(client_.statistics_volume(vname,
                                             false));

    expect_nothing(client_.statistics_node(local_node_id().str(),
                                           false));
}

TEST_F(PythonClientTest, redirect)
{
    mount_remote();

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         umount_remote();
                                     }));

    const uint64_t vsize = 1ULL << 20;
    const FrontendPath fname(make_volume_name("/some-volume"));
    const ObjectId id(create_file(fname, vsize));

    const XMLRPCVolumeInfo local_info(client_.info_volume(id));

    PythonClient remote_client(vrouter_cluster_id(),
                               {{address(), remote_config().xmlrpc_port}});
    const XMLRPCVolumeInfo remote_info(remote_client.info_volume(id));

#define EQ_(x)                                  \
    EXPECT_TRUE(local_info.x == remote_info.x)

    EQ_(volume_id);
    EQ_(_namespace_);
    EQ_(parent_namespace);
    EQ_(parent_snapshot_id);
    EQ_(volume_size);
    EQ_(lba_size);
    EQ_(cluster_multiplier);
    EQ_(sco_multiplier);
    EQ_(failover_mode);
    EQ_(failover_ip);
    EQ_(failover_port);
    EQ_(halted);
    EQ_(footprint);
    EQ_(stored);
    EQ_(object_type);
    EQ_(parent_volume_id);
    EQ_(vrouter_id);

    // operator== is gone
    // EXPECT_TRUE(local_info == remote_info);

#undef EQ_

    const ObjectId inexistent_id(yt::UUID().str());
    EXPECT_THROW(client_.info_volume(inexistent_id),
                 clienterrors::ObjectNotFoundException);
    EXPECT_THROW(remote_client.info_volume(inexistent_id),
                 clienterrors::ObjectNotFoundException);
}

TEST_F(PythonClientTest, redirection_response)
{
    // This test just checks whether we get a redirection response from the
    // server, for all supported calls.
    // As we set max_retries = 0 we don't try to contact the peer
    // (which isn't running anyhow), but at least we receive a
    // MaxRedirectsExceededException

    PythonClient client(vrouter_cluster_id(),
                        {{address(), local_config().xmlrpc_port}},
                        boost::none,
                        0);
    ObjectId dummy_volume("dummy");

    auto registry(fs_->object_router().object_registry());

    bpt::ptree pt;
    std::shared_ptr<yt::LockedArakoon>
        larakoon(new Registry(make_registry_config_(pt),
                              RegisterComponent::F));
    OwnerTagAllocator owner_tag_allocator(vrouter_cluster_id(),
                                          larakoon);

    const ObjectRegistration reg(vd::Namespace(),
                                 dummy_volume,
                                 remote_config().vrouter_id,
                                 ObjectTreeConfig::makeBase(),
                                 owner_tag_allocator(),
                                 FailOverCacheConfigMode::Automatic);

    registry->TESTONLY_add_to_registry_(reg);

    auto exit(yt::make_scope_exit([&]
                                  {
                                      registry->migrate(dummy_volume,
                                                        remote_config().vrouter_id,
                                                        local_config().vrouter_id);
                                      registry->unregister(dummy_volume);
                                  }));

#define CHECK_REDIRECT(call)                                            \
    try                                                                 \
    {                                                                   \
        call;                                                           \
        ADD_FAILURE() << #call " No exception thrown, MaxRedirectsExceededException expected"; \
    }                                                                   \
    catch (clienterrors::MaxRedirectsExceededException& e)              \
    {                                                                   \
        EXPECT_EQ(remote_config().xmlrpc_host, e.host);                 \
        EXPECT_EQ(remote_config().xmlrpc_port, e.port);                 \
    }                                                                   \
    CATCH_STD_ALL_EWHAT({                                               \
            ADD_FAILURE() << #call <<                                   \
                "expected MaxRedirectsExceededException, but got different exception: " << \
                EWHAT;                                                  \
        })

    //redirection based on volumeID
        CHECK_REDIRECT(client.list_snapshots(dummy_volume));
        CHECK_REDIRECT(client.info_snapshot(dummy_volume,
                                            "non-existing snapshot"));
        CHECK_REDIRECT(client.info_volume(dummy_volume));
        CHECK_REDIRECT(client.statistics_volume(dummy_volume));
        CHECK_REDIRECT(client.create_snapshot(dummy_volume));
        CHECK_REDIRECT(client.rollback_volume(dummy_volume,
                                              "non-existing snapshot"));
        CHECK_REDIRECT(client.delete_snapshot(dummy_volume,
                                              "non-existing snapshot"));
        CHECK_REDIRECT(client.set_volume_as_template(dummy_volume));
        CHECK_REDIRECT(client.get_scrubbing_work(dummy_volume));
        CHECK_REDIRECT(client.set_cluster_cache_behaviour(dummy_volume,
                                                          boost::none));
        CHECK_REDIRECT(client.get_cluster_cache_behaviour(dummy_volume));
        CHECK_REDIRECT(client.set_cluster_cache_mode(dummy_volume,
                                                     boost::none));
        CHECK_REDIRECT(client.get_cluster_cache_mode(dummy_volume));
        CHECK_REDIRECT(client.set_cluster_cache_limit(dummy_volume,
                                                      boost::none));
        CHECK_REDIRECT(client.get_cluster_cache_limit(dummy_volume));

        CHECK_REDIRECT(client.get_sync_ignore(dummy_volume));
        CHECK_REDIRECT(client.set_sync_ignore(dummy_volume, 10, 300));
        CHECK_REDIRECT(client.get_sco_multiplier(dummy_volume));
        CHECK_REDIRECT(client.set_sco_multiplier(dummy_volume, 1024));
        CHECK_REDIRECT(client.get_tlog_multiplier(dummy_volume));
        CHECK_REDIRECT(client.set_tlog_multiplier(dummy_volume, 1024));
        CHECK_REDIRECT(client.get_sco_cache_max_non_disposable_factor(dummy_volume));
        CHECK_REDIRECT(client.set_sco_cache_max_non_disposable_factor(dummy_volume, 12.0F));
        CHECK_REDIRECT(client.set_manual_failover_cache_config(dummy_volume,
                                                               boost::none));
        CHECK_REDIRECT(client.set_automatic_failover_cache_config(dummy_volume));
        CHECK_REDIRECT(client.get_failover_cache_config(dummy_volume));
        CHECK_REDIRECT(client.schedule_backend_sync(dummy_volume));
        CHECK_REDIRECT(client.is_volume_synced_up_to_tlog(dummy_volume,
                                                          boost::lexical_cast<std::string>(vd::TLogId())));
        CHECK_REDIRECT(client.is_volume_synced_up_to_snapshot(dummy_volume,
                                                              "some-snapshot"));
        CHECK_REDIRECT(client.set_metadata_cache_capacity(dummy_volume,
                                                          boost::none));
        CHECK_REDIRECT(client.get_metadata_cache_capacity(dummy_volume));

        //redirection based on nodeID
        CHECK_REDIRECT(client.migrate("non-existing volume",
                                      remote_node_id()));
        CHECK_REDIRECT(client.update_cluster_node_configs(remote_node_id()));

#undef CHECK_REDIRECT
}

TEST_F(PythonClientTest, max_redirects)
{
    test_redirects(client_.max_redirects);
}

TEST_F(PythonClientTest, max_redirects_exceeded)
{
    test_redirects(client_.max_redirects + 1);
}

TEST_F(PythonClientTest, set_as_template)
{
    const FrontendPath vpath(make_volume_name("/test_template"));
    const std::string vname(create_file(vpath, 10 << 20));

    EXPECT_EQ(ObjectType::Volume, client_.info_volume(vname).object_type);
    EXPECT_NO_THROW(client_.set_volume_as_template(vname));
    EXPECT_EQ(ObjectType::Template, client_.info_volume(vname).object_type);

    //testing idempotency
    EXPECT_NO_THROW(client_.set_volume_as_template(vname));

    ASSERT_THROW(client_.create_snapshot(vname), clienterrors::InvalidOperationException);
    ASSERT_THROW(client_.get_scrubbing_work(vname), clienterrors::InvalidOperationException);
}

TEST_F(PythonClientTest, revision_info)
{
    std::string server_version_str;
    std::string client_version_str;
    ASSERT_NO_THROW(server_version_str = client_.server_revision());
    ASSERT_NO_THROW(client_version_str = client_.client_revision());
    EXPECT_TRUE(not server_version_str.empty());
    EXPECT_EQ(server_version_str, client_version_str);
}

TEST_F(PythonClientTest, scrubbing)
{
    const FrontendPath vpath(make_volume_name("/testing_the_scrubber"));
    const ObjectId vol_id(create_file(vpath, 10 << 20));
    const off_t off = 10 * get_cluster_size(vol_id);
    const uint64_t size = 30 * get_cluster_size(vol_id);


    const std::string pattern("some_write");
    const uint32_t snapshot_num = 10;

    std::vector<std::string> snapshot_names;
    for (uint32_t i = 0; i < snapshot_num; i++)
    {
        write_to_file(vpath, pattern, size, off);
        snapshot_names.push_back(client_.create_snapshot(vol_id));
        wait_for_snapshot(vol_id, snapshot_names.back());
    }

    auto scrub_workitems = client_.get_scrubbing_work(vol_id);
    ASSERT_EQ(snapshot_num,
              scrub_workitems.size());

    client_.delete_snapshot(vol_id, snapshot_names[2]);

    //successful scrub expected
    client_.apply_scrubbing_result(scrub_wrap(scrub_workitems[1]));

    //one snapshot deleted, one scrubbed
    ASSERT_EQ(snapshot_num - 2,
              client_.get_scrubbing_work(vol_id).size());

    //deleted snapshot before scrubbing
    ASSERT_THROW(scrub_wrap(scrub_workitems[2]),
                 std::exception);

    // deleted snapshot after scrubbing but before applyscrubbing
    // ... the ScrubManager will notice.
    {
        auto result = scrub_wrap(scrub_workitems[3]);
        client_.delete_snapshot(vol_id, snapshot_names[3]);
        ScrubManager& sm = local_node(fs_->object_router())->scrub_manager();
        EXPECT_EQ(0,
                  sm.get_counters().parent_scrubs_nok);

        EXPECT_NO_THROW(client_.apply_scrubbing_result(result));

        while (not sm.get_parent_scrubs().empty())
        {
            boost::this_thread::sleep_for(boost::chrono::seconds(scrub_manager_interval_secs_));
        }

        EXPECT_EQ(1,
                  sm.get_counters().parent_scrubs_nok);
    }

    uint32_t togo = client_.get_scrubbing_work(vol_id).size();
    //two snapshots deleted, one scrubbed
    ASSERT_EQ(snapshot_num - 3, togo);

    //happy-path cleanup expected
    while(true)
    {
        auto scrub_workitems = client_.get_scrubbing_work(vol_id);
        ASSERT_EQ(togo, scrub_workitems.size());
        if (togo == 0)
        {
            break;
        }
        client_.apply_scrubbing_result(scrub_wrap(scrub_workitems[0]));
        togo--;
    }
}

TEST_F(PythonClientTest, volume_creation)
{
    const FrontendPath vpath("/volume");
    const yt::DimensionedValue size("1GiB");
    const ObjectId cname(client_.create_volume(vpath.str(),
                                               make_metadata_backend_config(),
                                               size));

    const XMLRPCVolumeInfo info(client_.info_volume(cname.str()));
    EXPECT_EQ(ObjectType::Volume, info.object_type);
    EXPECT_EQ(local_node_id(), NodeId(info.vrouter_id));
}

TEST_F(PythonClientTest, volume_creation_again)
{
    const FrontendPath vpath("/volume");
    const yt::DimensionedValue size("0B");
    const ObjectId vname(client_.create_volume(vpath.str(),
                                               nullptr,
                                               size));

    const XMLRPCVolumeInfo info(client_.info_volume(vname.str()));
    EXPECT_EQ(ObjectType::Volume, info.object_type);
    EXPECT_EQ(local_node_id(), NodeId(info.vrouter_id));
}

TEST_F(PythonClientTest, volume_creation_and_removal)
{
    const FrontendPath vpath("/volume");
    const yt::DimensionedValue size("0B");
    const ObjectId vname(client_.create_volume(vpath.str(),
                                               nullptr,
                                               size));

    const XMLRPCVolumeInfo info(client_.info_volume(vname.str()));
    EXPECT_EQ(ObjectType::Volume, info.object_type);
    EXPECT_EQ(local_node_id(), NodeId(info.vrouter_id));

    EXPECT_NO_THROW(client_.unlink(vpath.str()));
    EXPECT_THROW(client_.unlink(vpath.str()),
                 std::exception);
}

TEST_F(PythonClientTest, volume_resize)
{
    const FrontendPath vpath("/volume");
    const yt::DimensionedValue size("0B");
    const ObjectId vname(client_.create_volume(vpath.str(),
                                               nullptr,
                                               size));

    {
        const XMLRPCVolumeInfo info(client_.info_volume(vname.str()));
        EXPECT_EQ(ObjectType::Volume, info.object_type);
        EXPECT_EQ(0, info.volume_size);
    }

    const yt::DimensionedValue newsize("1 GiB");
    EXPECT_NO_THROW(client_.resize(vname.str(),
                                   newsize));

    {
        const XMLRPCVolumeInfo info(client_.info_volume(vname.str()));
        EXPECT_EQ(newsize.getBytes(),
                  info.volume_size);
    }

    EXPECT_THROW(client_.resize(vname.str(),
                                yt::DimensionedValue("500 MiB")),
                 std::exception);
}

TEST_F(PythonClientTest, clone_from_template)
{
    const FrontendPath tpath(make_volume_name("/template"));
    const ObjectId tname(create_file(tpath));

    const FrontendPath cpath("/clone");
    ASSERT_THROW(client_.create_clone_from_template(cpath.str(),
                                                    make_metadata_backend_config(),
                                                    tname.str()),
                 clienterrors::InvalidOperationException);

    client_.set_volume_as_template(tname.str());
    const ObjectId cname(client_.create_clone_from_template(cpath.str(),
                                                            make_metadata_backend_config(),
                                                            tname.str()));

    const XMLRPCVolumeInfo info(client_.info_volume(cname.str()));
    EXPECT_EQ(ObjectType::Volume, info.object_type);
    EXPECT_EQ(tname, ObjectId(info.parent_volume_id));
    EXPECT_EQ(local_node_id(), NodeId(info.vrouter_id));
}

TEST_F(PythonClientTest, clone)
{
    const FrontendPath ppath(make_volume_name("/parent"));
    const ObjectId pname(create_file(ppath));

    const vd::SnapshotName snap("snapshot");
    EXPECT_EQ(snap, client_.create_snapshot(pname,
                                            snap));
    wait_for_snapshot(pname,
                      snap);

    const FrontendPath cpath("/clone");
    const ObjectId cname(client_.create_clone(cpath.str(),
                                              make_metadata_backend_config(),
                                              pname,
                                              snap));

    const XMLRPCVolumeInfo info(client_.info_volume(cname.str()));
    EXPECT_EQ(ObjectType::Volume, info.object_type);
    EXPECT_EQ(pname, ObjectId(info.parent_volume_id));
    EXPECT_EQ(local_node_id(), NodeId(info.vrouter_id));
}

TEST_F(PythonClientTest, prevent_orphaned_clones)
{
    const FrontendPath ppath(make_volume_name("/parent"));
    const ObjectId pname(create_file(ppath));

    const vd::SnapshotName snap("snapshot");
    EXPECT_EQ(snap, client_.create_snapshot(pname,
                                            snap));
    wait_for_snapshot(pname,
                      snap);

    auto check_snap([&]
                    {
                        std::list<vd::SnapshotName> l;

                        LOCKVD();
                        api::showSnapshots(static_cast<const vd::VolumeId>(pname),
                                           l);

                        ASSERT_EQ(1U, l.size());
                        ASSERT_EQ(snap, l.front());
                    });

    check_snap();

    const FrontendPath cpath1("/clone-1");
    const ObjectId cname1(client_.create_clone(cpath1.str(),
                                               make_metadata_backend_config(),
                                               pname,
                                               snap));

    const FrontendPath cpath2("/clone-2");
    const ObjectId cname2(client_.create_clone(cpath2.str(),
                                               make_metadata_backend_config(),
                                               pname,
                                               snap));

    EXPECT_THROW(client_.delete_snapshot(pname,
                                         snap),
                 clienterrors::ObjectStillHasChildrenException);
    check_snap();
    EXPECT_NE(0, unlink(ppath));
    check_snap();

    EXPECT_EQ(0, unlink(clone_path_to_volume_path(cpath1)));
    EXPECT_EQ(0, unlink(cpath1));

    EXPECT_THROW(client_.delete_snapshot(pname,
                                         snap),
                 clienterrors::ObjectStillHasChildrenException);
    check_snap();
    EXPECT_NE(0, unlink(ppath));
    check_snap();

    EXPECT_EQ(0, unlink(clone_path_to_volume_path(cpath2)));
    EXPECT_EQ(0, unlink(cpath2));

    EXPECT_NO_THROW(client_.delete_snapshot(pname,
                                            snap));

    EXPECT_EQ(0, unlink(ppath));

    LOCKVD();
    ASSERT_THROW(api::getVolumePointer(static_cast<const vd::VolumeId>(pname)),
                 std::exception);
}

TEST_F(PythonClientTest, prevent_rollback_beyond_clone)
{
    const FrontendPath ppath(make_volume_name("/parent"));
    const ObjectId pname(create_file(ppath, 1ULL << 20));

    std::vector<vd::SnapshotName> snaps;
    snaps.reserve(4);

    const off_t off = get_cluster_size(pname);
    const uint64_t size = get_cluster_size(pname);

    for (size_t i = 0; i < snaps.capacity(); ++i)
    {
        const vd::SnapshotName snap("iteration" + boost::lexical_cast<std::string>(i));
        write_to_file(ppath, snap, size, off);
        client_.create_snapshot(pname, snap);
        snaps.emplace_back(snap);
        wait_for_snapshot(pname, snaps.back());
    }

    auto check_snaps([&](size_t snaps_left)
                     {
                         std::list<vd::SnapshotName> l;

                         {
                             LOCKVD();
                             api::showSnapshots(static_cast<const vd::VolumeId>(pname),
                                                l);
                         }

                         ASSERT_EQ(snaps_left, l.size());

                         auto it = l.begin();
                         for (size_t i = 0; i < snaps_left; ++i)
                         {
                             ASSERT_TRUE(it != l.end());
                             ASSERT_EQ(snaps.at(i), *it);
                             ++it;
                         }
                     });

    check_snaps(snaps.size());

    const FrontendPath cpath("/clone");
    const ObjectId cname(client_.create_clone(cpath.str(),
                                              make_metadata_backend_config(),
                                              pname,
                                              snaps.at(1)));

    check_file(clone_path_to_volume_path(cpath),
               snaps.at(1),
               size,
               off);

    EXPECT_NO_THROW(client_.rollback_volume(pname,
                                            snaps.at(2)));
    check_snaps(3);

    EXPECT_NO_THROW(client_.rollback_volume(pname,
                                            snaps.at(1)));
    check_snaps(2);

    EXPECT_THROW(client_.rollback_volume(pname,
                                         snaps.at(0)),
                 clienterrors::ObjectStillHasChildrenException);

    check_snaps(2);

    check_file(clone_path_to_volume_path(cpath),
               snaps.at(1),
               size,
               off);

    EXPECT_EQ(0, unlink(clone_path_to_volume_path(cpath)));
    EXPECT_EQ(0, unlink(cpath));

    EXPECT_NO_THROW(client_.rollback_volume(pname,
                                            snaps.at(0)));

    check_snaps(1);
}

TEST_F(PythonClientTest, family_scrubbing)
{
    const FrontendPath ppath(make_volume_name("/parent"));
    const size_t vsize = 10 << 20;

    const ObjectId pname(create_file(ppath,
                                     vsize));

    const size_t csize = get_cluster_size(pname);

    const std::string pattern("Blackstar");
    for (size_t i = 0; i < 64; ++i)
    {
        write_to_file(pname,
                      pattern,
                      csize,
                      0);
    }

    const vd::SnapshotName snap("snapshot");
    EXPECT_EQ(snap, client_.create_snapshot(pname,
                                            snap));
    wait_for_snapshot(pname,
                      snap);

    const FrontendPath cpath("/clone");
    const ObjectId cname(client_.create_clone(cpath.str(),
                                              make_metadata_backend_config(),
                                              pname,
                                              snap));

    const std::vector<std::string> scrub_work(client_.get_scrubbing_work(pname));
    ASSERT_EQ(1,
              scrub_work.size());

    client_.apply_scrubbing_result(scrub_wrap(scrub_work[0]));

    ScrubManager& sm = local_node(fs_->object_router())->scrub_manager();

    while (not sm.get_parent_scrubs().empty() or
           not sm.get_clone_scrubs().empty())
    {
        boost::this_thread::sleep_for(boost::chrono::seconds(scrub_manager_interval_secs_));
    }

    const ScrubManager::Counters c(sm.get_counters());
    EXPECT_EQ(1,
              c.parent_scrubs_ok);
    EXPECT_EQ(0,
              c.parent_scrubs_nok);
    EXPECT_EQ(1,
              c.clone_scrubs_ok);
    EXPECT_EQ(0,
              c.clone_scrubs_nok);
}

TEST_F(PythonClientTest, templates_and_scrubbing)
{
    const FrontendPath ppath(make_volume_name("/parent"));
    const size_t vsize = 10 << 20;

    const ObjectId pname(create_file(ppath,
                                     vsize));

    const size_t csize = get_cluster_size(pname);

    const std::string pattern("White Chalk");
    for (size_t i = 0; i < 64; ++i)
    {
        write_to_file(pname,
                      pattern,
                      csize,
                      0);
    }

    const vd::SnapshotName snap("snapshot");
    EXPECT_EQ(snap, client_.create_snapshot(pname,
                                            snap));
    wait_for_snapshot(pname,
                      snap);

    const std::vector<std::string> scrub_work(client_.get_scrubbing_work(pname));
    ASSERT_EQ(1,
              scrub_work.size());

    client_.set_volume_as_template(pname);
    EXPECT_EQ(ObjectType::Template,
              client_.info_volume(pname).object_type);

    // no more scrub work from a template
    EXPECT_THROW(client_.get_scrubbing_work(pname),
                 std::exception);

    // queuing it up works, the ScrubManager will however not be able to apply it
    client_.apply_scrubbing_result(scrub_wrap(scrub_work[0]));

    ScrubManager& sm = local_node(fs_->object_router())->scrub_manager();

    while (not sm.get_parent_scrubs().empty() or
           not sm.get_clone_scrubs().empty())
    {
        boost::this_thread::sleep_for(boost::chrono::seconds(scrub_manager_interval_secs_));
    }

    const ScrubManager::Counters c(sm.get_counters());
    EXPECT_EQ(0,
              c.parent_scrubs_ok);
    EXPECT_EQ(1,
              c.parent_scrubs_nok);
    EXPECT_EQ(0,
              c.clone_scrubs_ok);
    EXPECT_EQ(0,
              c.clone_scrubs_nok);
}

TEST_F(PythonClientTest, no_templating_of_files)
{
    const FrontendPath fpath("/file");
    const ObjectId fname(create_file(fpath));
    EXPECT_THROW(client_.set_volume_as_template(fname),
                 clienterrors::InvalidOperationException);
}

TEST_F(PythonClientTest, no_clone_from_file)
{
    const FrontendPath fpath("/file");
    const ObjectId fname(create_file(fpath));

    const FrontendPath cpath("/clone");
    EXPECT_THROW(client_.create_clone_from_template(cpath.str(),
                                                    make_metadata_backend_config(),
                                                    fname),
                 clienterrors::InvalidOperationException);

    const vd::SnapshotName snap("snap");
    EXPECT_THROW(client_.create_clone(cpath.str(),
                                      make_metadata_backend_config(),
                                      fname,
                                      snap),
                 clienterrors::InvalidOperationException);
}

TEST_F(PythonClientTest, no_clone_from_template_with_snapshot)
{
    const FrontendPath ppath(make_volume_name("/parent"));
    const ObjectId pname(create_file(ppath));

    const vd::SnapshotName snap("snapshot");
    EXPECT_EQ(snap, client_.create_snapshot(pname,
                                            snap));
    wait_for_snapshot(pname,
                      snap);

    client_.set_volume_as_template(pname.str());

    const FrontendPath cpath("/clone");

    EXPECT_THROW(client_.create_clone(cpath.str(),
                                      make_metadata_backend_config(),
                                      pname,
                                      snap),
                 clienterrors::InvalidOperationException);
}

TEST_F(PythonClientTest, remote_clone)
{
    mount_remote();

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         umount_remote();
                                     }));

    const FrontendPath tpath(make_volume_name("/template"));
    const ObjectId tname(create_file(tpath));

    client_.set_volume_as_template(tname.str());

    const FrontendPath cpath("/clone");
    const ObjectId cname(client_.create_clone_from_template(cpath.str(),
                                                            make_metadata_backend_config(),
                                                            tname.str(),
                                                            remote_node_id()));

    const XMLRPCVolumeInfo info(client_.info_volume(cname.str()));
    EXPECT_EQ(ObjectType::Volume, info.object_type);
    EXPECT_EQ(tname, ObjectId(info.parent_volume_id));
    EXPECT_EQ(remote_node_id(), NodeId(info.vrouter_id));
}

TEST_F(PythonClientTest, volume_and_object_id)
{
    {
        const FrontendPath vpath(make_volume_name("/some-volume"s));
        const ObjectId vname(create_file(vpath, 10 << 20));

        const boost::optional<vd::VolumeId>
            maybe_vid(client_.get_volume_id(vpath.str()));
        ASSERT_NE(boost::none,
                  maybe_vid);
        EXPECT_EQ(vname,
                  *maybe_vid);

        const boost::optional<ObjectId>
            maybe_oid(client_.get_object_id(vpath.str()));
        ASSERT_NE(boost::none,
                  maybe_oid);
        EXPECT_EQ(vname,
                  *maybe_oid);
    }

    {
        const FrontendPath fpath("/some-file");
        const ObjectId fname(create_file(fpath));

        ASSERT_EQ(boost::none,
                  client_.get_volume_id(fpath.str()));

        const boost::optional<ObjectId>
            maybe_oid(client_.get_object_id(fpath.str()));
        ASSERT_NE(boost::none,
                  maybe_oid);
        EXPECT_EQ(fname,
                  *maybe_oid);
    }

    {
        const FrontendPath nopath("/does-not-exist");
        verify_absence(nopath);

        ASSERT_EQ(boost::none,
                  client_.get_volume_id(nopath.str()));
        ASSERT_EQ(boost::none,
                  client_.get_object_id(nopath.str()));
    }
}

TEST_F(PythonClientTest, mds_management)
{
    const mds::ServerConfig scfg1(mds_test_setup_->next_server_config());
    mds_manager_->start_one(scfg1);

    const mds::ServerConfig scfg2(mds_test_setup_->next_server_config());
    mds_manager_->start_one(scfg2);

    ASSERT_NE(scfg1,
              scfg2);

    const FrontendPath vpath("/volume");
    const yt::DimensionedValue size("1GiB");

    const vd::MDSNodeConfigs ncfgs{ scfg1.node_config,
            scfg2.node_config };

    boost::shared_ptr<vd::MDSMetaDataBackendConfig>
        mcfg(new vd::MDSMetaDataBackendConfig(ncfgs,
                                              vd::ApplyRelocationsToSlaves::T));

    const ObjectId vname(client_.create_volume(vpath.str(),
                                               mcfg,
                                               size));

    auto check([&](const vd::MDSNodeConfigs& ref)
               {
                   const XMLRPCVolumeInfo
                       info(client_.info_volume(vname.str()));

                   auto c(boost::dynamic_pointer_cast<const vd::MDSMetaDataBackendConfig>(info.metadata_backend_config));

                   ASSERT_TRUE(ref == c->node_configs());
               });

    check(ncfgs);

    client_.update_metadata_backend_config(vname.str(),
                                           mcfg);

    check(ncfgs);

    const vd::MDSNodeConfigs ncfgs2{ scfg2.node_config,
            scfg1.node_config };

    boost::shared_ptr<vd::MDSMetaDataBackendConfig>
        mcfg2(new vd::MDSMetaDataBackendConfig(ncfgs2,
                                               vd::ApplyRelocationsToSlaves::T));

    client_.update_metadata_backend_config(vname.str(),
                                           mcfg2);

    check(ncfgs2);
}

TEST_F(PythonClientTest, sync_ignore)
{
    const FrontendPath vpath(make_volume_name("/syncignore-test"));
    const std::string vname(create_file(vpath, 10 << 20));
    auto res = client_.get_sync_ignore(vname);

    uint64_t number_of_syncs_to_ignore = bpy::extract<uint64_t>(res[XMLRPCKeys::number_of_syncs_to_ignore]);

    uint64_t maximum_time_to_ignore_syncs_in_seconds = bpy::extract<uint64_t>(res[XMLRPCKeys::maximum_time_to_ignore_syncs_in_seconds]);

    EXPECT_EQ(0U, number_of_syncs_to_ignore);
    EXPECT_EQ(0U, maximum_time_to_ignore_syncs_in_seconds);

    const uint64_t number_of_syncs_to_ignore_c = 234;
    const uint64_t maximum_time_to_ignore_syncs_in_seconds_c = 3234;

    client_.set_sync_ignore(vname,
                            number_of_syncs_to_ignore_c,
                            maximum_time_to_ignore_syncs_in_seconds_c);

    res = client_.get_sync_ignore(vname);

    number_of_syncs_to_ignore = bpy::extract<uint64_t>(res[XMLRPCKeys::number_of_syncs_to_ignore]);
    maximum_time_to_ignore_syncs_in_seconds = bpy::extract<uint64_t>(res[XMLRPCKeys::maximum_time_to_ignore_syncs_in_seconds]);

    EXPECT_EQ(number_of_syncs_to_ignore_c, number_of_syncs_to_ignore);
    EXPECT_EQ(maximum_time_to_ignore_syncs_in_seconds_c, maximum_time_to_ignore_syncs_in_seconds);

    client_.set_sync_ignore(vname,
                            0,
                            0);

    res = client_.get_sync_ignore(vname);
    number_of_syncs_to_ignore = bpy::extract<uint64_t>(res[XMLRPCKeys::number_of_syncs_to_ignore]);
    maximum_time_to_ignore_syncs_in_seconds = bpy::extract<uint64_t>(res[XMLRPCKeys::maximum_time_to_ignore_syncs_in_seconds]);

    EXPECT_EQ(0U, number_of_syncs_to_ignore);
    EXPECT_EQ(0U, maximum_time_to_ignore_syncs_in_seconds);
}

TEST_F(PythonClientTest, sco_multiplier)
{
    const FrontendPath vpath(make_volume_name("/sco_multiplier-test"));
    const std::string vname(create_file(vpath, 10 << 20));

    uint32_t sco_multiplier = client_.get_sco_multiplier(vname);
    EXPECT_EQ(1024U, sco_multiplier);

    const uint32_t sco_multiplier_c = sco_multiplier + 1;
    client_.set_sco_multiplier(vname, sco_multiplier_c);
    sco_multiplier = client_.get_sco_multiplier(vname);
    EXPECT_EQ(sco_multiplier_c, sco_multiplier);

    EXPECT_THROW(client_.set_sco_multiplier(vname, 1),
                 clienterrors::InvalidOperationException);

    // could throw an InvalidOperationException or an InsufficientResourcesException
    // depending on the available SCOCache space
    EXPECT_THROW(client_.set_sco_multiplier(vname, 32769),
                 std::exception);
}

TEST_F(PythonClientTest, tlog_multiplier)
{
    const FrontendPath vpath(make_volume_name("/tlog_multiplier-test"));
    const std::string vname(create_file(vpath, 10 << 20));

    EXPECT_EQ(boost::none,
              client_.get_tlog_multiplier(vname));

    const uint32_t tlog_multiplier_c = 50;
    client_.set_tlog_multiplier(vname, tlog_multiplier_c);
    const boost::optional<uint32_t>
        tlog_multiplier(client_.get_tlog_multiplier(vname));

    ASSERT_NE(boost::none,
              tlog_multiplier);

    EXPECT_EQ(tlog_multiplier_c,
              *tlog_multiplier);

    client_.set_tlog_multiplier(vname,
                                boost::none);
    EXPECT_EQ(boost::none,
              client_.get_tlog_multiplier(vname));
}

TEST_F(PythonClientTest, max_non_disposable_factor)
{
    const FrontendPath vpath(make_volume_name("/max_non_disposable_factor-test"));
    const std::string vname(create_file(vpath, 10 << 20));

    EXPECT_EQ(boost::none,
              client_.get_sco_cache_max_non_disposable_factor(vname));

    const float max_non_disposable_factor_c = 12.0F;
    client_.set_sco_cache_max_non_disposable_factor(vname,
                                                    max_non_disposable_factor_c);

    const boost::optional<float>
        max_non_disposable_factor(client_.get_sco_cache_max_non_disposable_factor(vname));
    ASSERT_NE(boost::none,
              max_non_disposable_factor);

    EXPECT_EQ(max_non_disposable_factor_c,
              *max_non_disposable_factor);

    EXPECT_THROW(client_.set_sco_cache_max_non_disposable_factor(vname,
                                                                 0.99F),
                 clienterrors::InvalidOperationException);

    client_.set_sco_cache_max_non_disposable_factor(vname,
                                                    boost::none);

    EXPECT_EQ(boost::none,
              client_.get_sco_cache_max_non_disposable_factor(vname));
}

TEST_F(PythonClientTest, cluster_cache_behaviour)
{
    const FrontendPath vpath(make_volume_name("/cluster-cache-behaviour-test"));
    const std::string vname(create_file(vpath, 10 << 20));

    ASSERT_EQ(boost::none,
              client_.get_cluster_cache_behaviour(vname));

    auto fun([&](const boost::optional<vd::ClusterCacheBehaviour>& b)
             {
                 client_.set_cluster_cache_behaviour(vname,
                                                     b);

                 ASSERT_EQ(b,
                           client_.get_cluster_cache_behaviour(vname));
             });

    fun(vd::ClusterCacheBehaviour::CacheOnRead);
    fun(vd::ClusterCacheBehaviour::CacheOnWrite);
    fun(vd::ClusterCacheBehaviour::NoCache);
    fun(boost::none);
}

TEST_F(PythonClientTest, cluster_cache_mode)
{
    const FrontendPath vpath(make_volume_name("/cluster-cache-mode-test"));
    const std::string vname(create_file(vpath, 10 << 20));

    ASSERT_EQ(boost::none,
              client_.get_cluster_cache_mode(vname));

    ASSERT_EQ(vd::ClusterCacheMode::ContentBased,
              vd::VolManager::get()->get_cluster_cache_default_mode());

    auto fun([&](const boost::optional<vd::ClusterCacheMode>& m)
             {
                 client_.set_cluster_cache_mode(vname,
                                                m);

                 ASSERT_EQ(m,
                           client_.get_cluster_cache_mode(vname));
             });

    fun(vd::ClusterCacheMode::ContentBased);
    fun(boost::none);
    fun(vd::ClusterCacheMode::LocationBased);

    EXPECT_THROW(fun(vd::ClusterCacheMode::ContentBased),
                 std::exception);

    EXPECT_EQ(vd::ClusterCacheMode::LocationBased,
              client_.get_cluster_cache_mode(vname));
}

TEST_F(PythonClientTest, cluster_cache_limit)
{
    const FrontendPath vpath(make_volume_name("/cluster-cache-mode-test"));
    const std::string vname(create_file(vpath, 10 << 20));

    ASSERT_EQ(boost::none,
              client_.get_cluster_cache_limit(vname));

    auto fun([&](const boost::optional<vd::ClusterCount>& l)
             {
                 client_.set_cluster_cache_limit(vname,
                                                 l);

                 const boost::optional<vd::ClusterCount>
                     m(client_.get_cluster_cache_limit(vname));
                 ASSERT_EQ(l,
                           m);
             });

    fun(vd::ClusterCount(1));

    EXPECT_THROW(fun(vd::ClusterCount(0)),
                 std::exception);

    EXPECT_EQ(vd::ClusterCount(1),
              *client_.get_cluster_cache_limit(vname));

    fun(boost::none);
}

TEST_F(PythonClientTest, update_cluster_node_configs)
{
    ASSERT_NO_THROW(client_.update_cluster_node_configs(std::string()));

    ObjectRouter& router = fs_->object_router();

    ASSERT_EQ(local_config(),
              router.node_config());
    ASSERT_EQ(remote_config(),
              *router.node_config(remote_config().vrouter_id));

    std::shared_ptr<ClusterRegistry> registry(router.cluster_registry());
    const ClusterNodeConfigs configs(registry->get_node_configs());

    registry->erase_node_configs();
    registry->set_node_configs({ router.node_config() });

    ASSERT_NO_THROW(client_.update_cluster_node_configs(std::string()));

    ASSERT_EQ(local_config(),
              router.node_config());
    ASSERT_EQ(boost::none,
              router.node_config(remote_config().vrouter_id));

    registry->erase_node_configs();
    registry->set_node_configs({ configs });

    ASSERT_NO_THROW(client_.update_cluster_node_configs(local_config().vrouter_id));

    ASSERT_EQ(local_config(),
              router.node_config());
    ASSERT_EQ(remote_config(),
              *router.node_config(remote_config().vrouter_id));
}

TEST_F(PythonClientTest, vaai_copy)
{
    EXPECT_TRUE(client_.list_volumes().empty());

    const FrontendPath vpath(make_volume_name("/vol"));
    const ObjectId vname(create_file(vpath, 10 << 20));

    const FrontendPath fc_vpath(make_volume_name("/vol-full-clone"));
    const ObjectId fc_vname(create_file(fc_vpath, 10 << 20));

    EXPECT_EQ(2, client_.list_volumes().size());

    const std::string src_path("/vol-flat.vmdk");
    const std::string fc_target_path("/vol-full-clone-flat.vmdk");
    const uint64_t timeout = 10;
    const CloneFileFlags fc_flags(CloneFileFlags::SkipZeroes);

    EXPECT_NO_THROW(client_.vaai_copy(src_path,
                                      fc_target_path,
                                      timeout,
                                      fc_flags));

    const CloneFileFlags lz_flags(CloneFileFlags::Lazy |
                                  CloneFileFlags::Guarded);

    const std::string lz_target_path("/vol-lazy-snapshot-flat.vmdk");
    EXPECT_NO_THROW(client_.vaai_copy(src_path,
                                      lz_target_path,
                                      timeout,
                                      lz_flags));

    const std::string nonexistent_target_path("/non-existent-volume-flat.vmdk");
    EXPECT_THROW(client_.vaai_copy(src_path,
                                   nonexistent_target_path,
                                   timeout,
                                   fc_flags),
                 clienterrors::ObjectNotFoundException);

    EXPECT_THROW(client_.vaai_copy(src_path,
                                   lz_target_path,
                                   timeout,
                                   lz_flags),
                 clienterrors::FileExistsException);

    const FrontendPath fc_size_mismatch(make_volume_name("/vol-size-mismatch"));
    const ObjectId sm_vname(create_file(fc_size_mismatch, 10 << 10));

    EXPECT_THROW(client_.vaai_copy(fc_size_mismatch.string(),
                                   fc_target_path,
                                   timeout,
                                   fc_flags),
                 clienterrors::InvalidOperationException);

    const std::string nonexistent_src_path("/non-existent-src-volume-flat.vmdk");
    EXPECT_THROW(client_.vaai_copy(nonexistent_src_path,
                                   fc_target_path,
                                   timeout,
                                   fc_flags),
                 clienterrors::ObjectNotFoundException);

    EXPECT_THROW(client_.vaai_copy(nonexistent_src_path,
                                   lz_target_path,
                                   timeout,
                                   lz_flags),
                 clienterrors::ObjectNotFoundException);

    const CloneFileFlags fault_flags(static_cast<CloneFileFlags>(0));
    const FrontendPath fc_vpath_2(make_volume_name("/vol-full-clone-2"));
    const ObjectId fc_vname_2(create_file(fc_vpath_2, 10 << 20));
    const std::string fc_target_path_2("/vol-full-clone-2-flat.vmdk");
    EXPECT_THROW(client_.vaai_copy(src_path,
                                   fc_target_path_2,
                                   timeout,
                                   fault_flags),
                 clienterrors::InvalidOperationException);

    const std::string src_vmdk_path("/vol.vmdk");
    const ObjectId fid(create_file(FrontendPath(src_vmdk_path),
                                   4096));
    const std::string non_existent_vmdk_path("/vol-non-existent.vmdk");
    EXPECT_NO_THROW(client_.vaai_copy(src_vmdk_path,
                                      non_existent_vmdk_path,
                                      timeout,
                                      lz_flags));
    /* Non-existent path now exists - call again the filecopy */
    EXPECT_NO_THROW(client_.vaai_copy(src_vmdk_path,
                                      non_existent_vmdk_path,
                                      timeout,
                                      lz_flags));

    EXPECT_THROW(client_.vaai_copy(src_vmdk_path,
                                   non_existent_vmdk_path,
                                   timeout,
                                   fault_flags),
                 clienterrors::InvalidOperationException);
}

TEST_F(PythonClientTest, failovercache_config)
{
    start_failovercache_for_remote_node();

    const FrontendPath vpath(make_volume_name("/failovercacheconfig-test"));
    const std::string vname(create_file(vpath, 10 << 20));

    const vd::FailOverCacheConfig cfg1(check_initial_foc_config(vname));

    check_foc_state(vname,
                    vd::VolumeFailOverState::OK_SYNC);

    //

    client_.set_manual_failover_cache_config(vname,
                                             boost::none);
    check_foc_config(vname,
                     FailOverCacheConfigMode::Manual,
                     boost::none);

    check_foc_state(vname,
                    vd::VolumeFailOverState::OK_STANDALONE);

    //

    const vd::FailOverCacheConfig cfg2(local_config().failovercache_host,
                                       local_config().failovercache_port,
                                       vd::FailOverCacheMode::Asynchronous);

    ASSERT_NE(cfg1,
              cfg2);

    client_.set_manual_failover_cache_config(vname,
                                             cfg2);

    check_foc_config(vname,
                     FailOverCacheConfigMode::Manual,
                     cfg2);

    check_foc_state(vname,
                    vd::VolumeFailOverState::OK_SYNC);

    //

    const vd::FailOverCacheConfig cfg3("somewhereoutthere"s,
                                       local_config().failovercache_port,
                                       vd::FailOverCacheMode::Asynchronous);

    client_.set_manual_failover_cache_config(vname,
                                             cfg3);

    check_foc_config(vname,
                     FailOverCacheConfigMode::Manual,
                     cfg3);

    check_foc_state(vname,
                    vd::VolumeFailOverState::DEGRADED);

    client_.set_automatic_failover_cache_config(vname);

    check_foc_config(vname,
                     FailOverCacheConfigMode::Automatic,
                     cfg1);

    check_foc_state(vname,
                    vd::VolumeFailOverState::OK_SYNC);

    //

    const vd::FailOverCacheConfig cfg4(cfg1.host,
                                       cfg1.port,
                                       vd::FailOverCacheMode::Synchronous);

    client_.set_manual_failover_cache_config(vname,
                                             cfg4);

    check_foc_config(vname,
                     FailOverCacheConfigMode::Manual,
                     cfg4);

    check_foc_state(vname,
                    vd::VolumeFailOverState::OK_SYNC);

}

TEST_F(PythonClientTest, locked_client)
{
    const FrontendPath vpath(make_volume_name("/some-volume"));
    const std::string vname(create_file(vpath, 10 << 20));

    int counter = 0;
    std::atomic<bool> stop(false);

    auto fun([&](int n)
             {
                 boost::shared_ptr<LockedPythonClient>
                     lclient(client_.make_locked_client(vname));

                 while (not stop)
                 {
                     lclient->enter();

                     bpy::object dummy;
                     auto on_exit(yt::make_scope_exit([&]
                                                      {
                                                          lclient->exit(dummy,
                                                                        dummy,
                                                                        dummy);
                                                      }));
                     counter = 0;

                     for (size_t i = 0; i < 1000000; ++i)
                     {
                         counter += n;
                         EXPECT_EQ(0,
                                   counter % n);
                     }
                 }
             });

    auto f1(std::async(std::launch::async,
                       [&]
                       {
                           fun(3);
                       }));

    auto f2(std::async(std::launch::async,
                       [&]
                       {
                           fun(5);
                       }));

    boost::this_thread::sleep_for(boost::chrono::seconds(10));
    stop.store(true);

    f1.wait();
    f2.wait();
}

TEST_F(PythonClientTest, locked_scrub)
{
    const FrontendPath vpath(make_volume_name("/some-volume"));
    const std::string vname(create_file(vpath, 10 << 20));

    const uint64_t csize = get_cluster_size(ObjectId(vname));
    const std::string fst("first");

    write_to_file(vpath,
                  fst,
                  csize,
                  0);

    const std::string snd("second");
    write_to_file(vpath,
                  snd,
                  csize,
                  0);

    const std::string snap(client_.create_snapshot(vname));

    wait_for_snapshot(ObjectId(vname),
                      snap);

    boost::shared_ptr<LockedPythonClient>
        lclient(client_.make_locked_client(vname)->enter());

    bpy::object dummy;
    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         lclient->exit(dummy,
                                                       dummy,
                                                       dummy);
                                     }));

    const std::vector<std::string> work(lclient->get_scrubbing_work());
    ASSERT_EQ(1,
              work.size());

    const fs::path tmp(topdir_ / "scrubscratchspace");
    fs::create_directories(tmp);

    const std::string res(lclient->scrub(work[0],
                                         tmp.string(),
                                         scrubbing::ScrubberAdapter::region_size_exponent_default,
                                         scrubbing::ScrubberAdapter::fill_ratio_default,
                                         scrubbing::ScrubberAdapter::verbose_scrubbing_default,
                                         "ovs_scrubber",
                                         yt::Severity::info,
                                         std::vector<std::string>(),
                                         configuration_.string()));

    lclient->apply_scrubbing_result(res);
}

TEST_F(PythonClientTest, backend_sync_tlog)
{
    const FrontendPath vpath(make_volume_name("/some-volume"));
    const std::string vname(create_file(vpath, 10 << 20));

    const vd::TLogName tlog_name(client_.schedule_backend_sync(vname));
    ASSERT_TRUE(vd::TLog::isTLogString(tlog_name));

    const size_t sleep_msecs = 100;
    const size_t count = 60 * 1000 / sleep_msecs;
    for (size_t i = 0; i < count; ++i)
    {
        if (client_.is_volume_synced_up_to_tlog(vname,
                                                tlog_name))
        {
            break;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));
        }
    }

    ASSERT_TRUE(client_.is_volume_synced_up_to_tlog(vname,
                                                    tlog_name));
}

TEST_F(PythonClientTest, backend_sync_snapshot)
{
    const FrontendPath vpath(make_volume_name("/some-volume"));
    const std::string vname(create_file(vpath, 10 << 20));

    const std::string snap("some-snapshot");
    EXPECT_THROW(client_.is_volume_synced_up_to_snapshot(vname,
                                                         snap),
                 std::exception);

    client_.create_snapshot(vname,
                            snap);

    const size_t sleep_msecs = 100;
    const size_t count = 60 * 1000 / sleep_msecs;
    for (size_t i = 0; i < count; ++i)
    {
        if (client_.is_volume_synced_up_to_snapshot(vname,
                                                    snap))
        {
            break;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_msecs));
        }
    }

    ASSERT_TRUE(client_.is_volume_synced_up_to_snapshot(vname,
                                                        snap));
}

TEST_F(PythonClientTest, metadata_cache_capacity)
{
    const FrontendPath vpath(make_volume_name("/some-volume"));
    const std::string vname(create_file(vpath, 10 << 20));

    EXPECT_EQ(boost::none,
              client_.get_metadata_cache_capacity(vname));

    EXPECT_THROW(client_.set_metadata_cache_capacity(vname,
                                                     boost::optional<size_t>(0)),
                 std::exception);

    EXPECT_EQ(boost::none,
              client_.get_metadata_cache_capacity(vname));

    client_.set_metadata_cache_capacity(vname,
                                        boost::optional<size_t>(256U));

    boost::optional<size_t> cap(client_.get_metadata_cache_capacity(vname));
    ASSERT_NE(boost::none,
              cap);

    EXPECT_EQ(256,
              *cap);

    client_.set_metadata_cache_capacity(vname,
                                        boost::none);
    EXPECT_EQ(boost::none,
              client_.get_metadata_cache_capacity(vname));
}

TEST_F(PythonClientTest, restart_volume)
{
    mount_remote();

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         umount_remote();
                                     }));

    const FrontendPath vname(make_volume_name("/volume"));
    const ObjectId oid(create_file(vname,
                                   1ULL << 20));

    const std::string pattern("a rather important message");

    write_to_file(vname,
                  pattern.c_str(),
                  pattern.size(),
                  0);

    PythonClient remote_client(vrouter_cluster_id(),
                               {{address(), remote_config().xmlrpc_port}});

    remote_client.stop_object(oid.str());

    std::vector<char> buf(pattern.size());

    ASSERT_GT(0,
              read_from_file(vname,
                             buf.data(),
                             buf.size(),
                             0));

    const std::string pattern2("a much less important message");
    ASSERT_GT(0,
              write_to_file(vname,
                            pattern2.c_str(),
                            pattern2.size(),
                            0));

    remote_client.restart_object(oid.str(),
                                 false);
    read_from_file(vname,
                   buf.data(),
                   buf.size(),
                   0);

    const std::string s(buf.data(),
                        buf.size());

    ASSERT_EQ(pattern,
              s);
}

// https://github.com/openvstorage/volumedriver/issues/258
TEST_F(PythonClientTest, list_snapshots_while_migrating)
{
    start_failovercache_for_remote_node();
    mount_remote();
    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         umount_remote();
                                     }));

    const uint64_t vsize = 1ULL << 20;
    const FrontendPath vname(make_volume_name("/some-volume"));
    const ObjectId id(create_file(vname, vsize));

    const std::string snap("snap");
    client_.create_snapshot(id, snap);
    wait_for_snapshot(id, snap);

    std::atomic<bool> stop(false);

    std::future<void>
        f(std::async(std::launch::async,
                     [&]
                     {
                         PythonClient
                             client(vrouter_cluster_id(),
                                    {
                                        { address(), remote_config().xmlrpc_port },
                                        { address(), local_config().xmlrpc_port }
                                    });
                         while (not stop)
                         {
                             try
                             {
                                 EXPECT_EQ(1,
                                           client.list_snapshots(id).size());
                             }
                             catch (clienterrors::VolumeRestartInProgressException&)
                             {}
                             catch (clienterrors::MaxRedirectsExceededException&)
                             {
                                 // it's been observed that the client redirection
                                 // sometimes loses the catch-up-with-the-volume-location
                                 // race.
                             }

                             std::this_thread::sleep_for(std::chrono::milliseconds(200));
                         }
                     }));

    for (size_t i = 0; i < 1; ++i)
    {
        client_.migrate(id,
                        remote_node_id());
        client_.migrate(id,
                        local_node_id());
    }

    stop = true;
    EXPECT_NO_THROW(f.get());
}

TEST_F(PythonClientTest, DISABLED_list_methods)
{
    const std::vector<std::string> ms(list_methods());
    for (const auto& m : ms)
    {
        std::cout << "method: " << m << std::endl;
    }
}

TEST_F(PythonClientTest, fallback)
{
    test_fallback();
}

TEST_F(PythonClientTest, sco_cache_mount_point_info)
{
    vd::SCOCacheMountPointsInfo map;
    api::getSCOCacheMountPointsInfo(map);

    const std::vector<vd::SCOCacheMountPointInfo>
        vec(client_.sco_cache_mount_point_info(local_node_id()));

    ASSERT_FALSE(vec.empty());
    ASSERT_EQ(map.size(),
              vec.size());

    for (const auto& mpinfo : vec)
    {
        auto it = map.find(mpinfo.path);
        ASSERT_TRUE(it != map.end());
        ASSERT_EQ(it->second,
                  mpinfo);
    }
}

TEST_F(PythonClientTest, fenced_volume_info)
{
    mount_remote();
    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         umount_remote();
                                     }));

    const FrontendPath path(make_volume_name("/volume"));
    const ObjectId oid(create_file(path));

    simulate_fencing(oid);

    const XMLRPCVolumeInfo local_info(client_.info_volume(oid.str(),
                                                          boost::none,
                                                          false));
    EXPECT_TRUE(local_info.halted);

    const XMLRPCVolumeInfo redirected_info(client_.info_volume(oid.str(),
                                                               boost::none,
                                                               true));
    EXPECT_FALSE(redirected_info.halted);

    PythonClient rclient(vrouter_cluster_id(),
                         {{ remote_config().xmlrpc_host,
                            remote_config().xmlrpc_port }});

    const XMLRPCVolumeInfo remote_info(rclient.info_volume(oid.str(),
                                                           boost::none,
                                                           false));
    EXPECT_FALSE(remote_info.halted);
}

TEST_F(PythonClientTest, redirection_on_fencing)
{
    mount_remote();
    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         umount_remote();
                                     }));

    const FrontendPath path(make_volume_name("/volume"));
    const ObjectId oid(create_file(path));

    simulate_fencing(oid);

    EXPECT_TRUE(client_.list_snapshots(oid.str()).empty());
    EXPECT_NO_THROW(client_.statistics_volume(oid.str()));
}

TEST_F(PythonClientTest, list_halted_volumes)
{
    const FrontendPath path(make_volume_name("/volume"));
    const ObjectId oid(create_file(path));

    EXPECT_TRUE(client_.list_halted_volumes(local_node_id()).empty());

    halt_volume(oid);

    const std::vector<std::string>
        vols(client_.list_halted_volumes(local_node_id()));

    ASSERT_FALSE(vols.empty());
    EXPECT_EQ(1, vols.size());
    EXPECT_EQ(oid,
              ObjectId(vols[0]));
}

TEST_F(PythonClientTest, halted_volume_exception)
{
    const FrontendPath path(make_volume_name("/volume"));
    const ObjectId oid(create_file(path));

    halt_volume(oid);

    EXPECT_THROW(client_.list_snapshots(oid.str()),
                 clienterrors::VolumeHaltedException);
    EXPECT_THROW(client_.statistics_volume(oid.str()),
                 clienterrors::VolumeHaltedException);
    EXPECT_NO_THROW(client_.info_volume(oid.str()));
}

}
