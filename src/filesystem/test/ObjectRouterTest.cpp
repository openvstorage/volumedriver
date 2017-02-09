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

#include <cppzmq/zmq.hpp>

#include <youtils/UUID.h>

#include <volumedriver/Api.h>

#include <filesystem/ZUtils.h>

#include "../LocalNode.h"
#include "../MessageUtils.h"
#include "../Protocol.h"
#include "../RemoteNode.h"

namespace volumedriverfstest
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

using namespace volumedriverfs;
using namespace std::literals::string_literals;

class ObjectRouterTest
    : public FileSystemTestBase
{
public:
    ObjectRouterTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("ObjectRouterTest"))
        , ztx(1)
        , zock(ztx, ZMQ_REQ)
        , next_port_off_(2)
    {
        const std::string addr("tcp://"s +
                               local_config().message_host +
                               ":"s +
                               boost::lexical_cast<std::string>(local_config().message_port));
        zock.connect(addr.c_str());
    }

    // Do we really want to assemble the message parts here?
    // (Try to) unify with the production client side code instead.
    std::string
    remote_read(const ObjectId& vname,
                uint64_t size,
                uint64_t off)
    {
        const Object obj(ObjectType::Volume,
                              vname);

        const auto rreq(vfsprotocol::MessageUtils::create_read_request(obj,
                                                                       size,
                                                                       off));
        ZUtils::serialize_to_socket(zock,
                                         vfsprotocol::RequestType::Read,
                                         MoreMessageParts::T);

        vfsprotocol::Tag tag(reinterpret_cast<uint64_t>(&rreq));

        ZUtils::serialize_to_socket(zock,
                                         tag,
                                         MoreMessageParts::T);

        ZUtils::serialize_to_socket(zock, rreq, MoreMessageParts::F);

        vfsprotocol::ResponseType rsp;
        ZUtils::deserialize_from_socket(zock, rsp);

        EXPECT_TRUE(rsp == vfsprotocol::ResponseType::Ok);

        ZEXPECT_MORE(zock, "tag");

        vfsprotocol::Tag rsp_tag;
        ZUtils::deserialize_from_socket(zock, rsp_tag);
        EXPECT_EQ(tag, rsp_tag);

        ZEXPECT_MORE(zock, "read data");

        zmq::message_t read_data;
        zock.recv(&read_data);

        EXPECT_EQ(size, read_data.size());
        const std::string s(static_cast<const char*>(read_data.data()), read_data.size());

        return s;
    }

    void
    remote_write(const ObjectId& vname,
                 const std::string& pattern,
                 uint64_t off)
    {
        const Object obj(ObjectType::Volume,
                              vname);

        const auto wreq(vfsprotocol::MessageUtils::create_write_request(obj,
                                                                        pattern.size(),
                                                                        off));

        ZUtils::serialize_to_socket(zock,
                                         vfsprotocol::RequestType::Write,
                                         MoreMessageParts::T);

        const vfsprotocol::Tag tag(reinterpret_cast<uint64_t>(&wreq));
        ZUtils::serialize_to_socket(zock,
                                         tag,
                                         MoreMessageParts::T);

        ZUtils::serialize_to_socket(zock,
                                         wreq,
                                         MoreMessageParts::T);

        zmq::message_t write_data(pattern.size());
        memcpy(write_data.data(), pattern.c_str(), pattern.size());
        zock.send(write_data, 0);

        vfsprotocol::ResponseType rsp;
        ZUtils::deserialize_from_socket(zock, rsp);

        EXPECT_TRUE(rsp == vfsprotocol::ResponseType::Ok);

        ZEXPECT_MORE(zock, "tag");

        vfsprotocol::Tag rsp_tag;
        ZUtils::deserialize_from_socket(zock, rsp_tag);

        EXPECT_EQ(tag, rsp_tag);

        ZEXPECT_MORE(zock, "WriteResponse");

        vfsprotocol::WriteResponse wrsp;
        ZUtils::deserialize_from_socket(zock, wrsp);

        EXPECT_EQ(pattern.size(), wrsp.size());

        ZEXPECT_NOTHING_MORE(zock);
    }

    void
    remote_sync(const ObjectId& vname)
    {
        const Object obj(ObjectType::Volume,
                         vname);

        const auto sreq(vfsprotocol::MessageUtils::create_sync_request(obj));

        ZUtils::serialize_to_socket(zock,
                                    vfsprotocol::RequestType::Sync,
                                    MoreMessageParts::T);

        const vfsprotocol::Tag tag(reinterpret_cast<uint64_t>(&sreq));
        ZUtils::serialize_to_socket(zock,
                                    tag,
                                    MoreMessageParts::T);

        ZUtils::serialize_to_socket(zock,
                                    sreq,
                                    MoreMessageParts::F);

        vfsprotocol::ResponseType rsp;
        ZUtils::deserialize_from_socket(zock, rsp);

        EXPECT_TRUE(rsp == vfsprotocol::ResponseType::Ok);

        ZEXPECT_MORE(zock, "tag");

        vfsprotocol::Tag rsp_tag;
        ZUtils::deserialize_from_socket(zock, rsp_tag);

        EXPECT_EQ(tag, rsp_tag);

        ZEXPECT_MORE(zock, "SyncResponse");

        vfsprotocol::SyncResponse srsp;
        ZUtils::deserialize_from_socket(zock, srsp);
        ZEXPECT_NOTHING_MORE(zock);

        EXPECT_TRUE(srsp.dtl_in_sync());
    }

    using NodeMap = ObjectRouter::NodeMap;
    using ConfigMap = ObjectRouter::ConfigMap;

    static const NodeMap&
    node_map(const ObjectRouter& r)
    {
        return r.node_map_;
    }

    static const ConfigMap&
    config_map(const ObjectRouter& r)
    {
        return r.config_map_;
    }

    std::pair<NodeMap, ConfigMap>
    build_config(ObjectRouter& r)
    {
        return r.build_config_(boost::none);
    }

    ClusterNodeConfig
    expand_cluster(ObjectRouter& router,
                   const NodeId& new_node_id)
    {
        std::shared_ptr<ClusterRegistry> registry(router.cluster_registry());

        ClusterNodeConfigs configs(registry->get_node_configs());

        const ClusterNodeConfig
            new_config(new_node_id,
                       local_config().message_host,
                       MessagePort(local_config().message_port + next_port_off_),
                       XmlRpcPort(local_config().xmlrpc_port + next_port_off_),
                       FailoverCachePort(local_config().failovercache_port + next_port_off_),
                       network_server_uri(local_node_id()),
                       local_config().xmlrpc_host,
                       local_config().failovercache_host);

        ++next_port_off_;

        configs.push_back(new_config);

        registry->erase_node_configs();
        registry->set_node_configs(configs);

        router.update_cluster_node_configs();

        return new_config;
    }

    bool
    steal(ObjectRouter& router,
          const ObjectRegistration& reg,
          const OnlyStealFromOfflineNode only_steal_from_offline_node)
    {
        return router.steal_(reg,
                             only_steal_from_offline_node,
                             ForceRestart::T);
    }

    zmq::context_t ztx;
    zmq::socket_t zock;
    uint16_t next_port_off_;
};

TEST_F(ObjectRouterTest, remote_read)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = get_cluster_size(vname) - 1;

    write_to_file(fname, pattern, pattern.size(), off);
    EXPECT_EQ(pattern, remote_read(vname, pattern.size(), off));
}

TEST_F(ObjectRouterTest, remote_write)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("remotely written");
    const uint64_t off = get_cluster_size(vname) - 1;

    remote_write(vname, pattern, off);
    check_file(fname, pattern, pattern.size(), off);
}

TEST_F(ObjectRouterTest, remote_sync)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    remote_sync(vname);
}

TEST_F(ObjectRouterTest, invalid_request_type)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = get_cluster_size(vname) - 1;

    write_to_file(fname, pattern, pattern.size(), off);

    const uint32_t req_type = 0;
    static_assert(sizeof(req_type) == sizeof(vfsprotocol::RequestType),
                  "fix yer test");

    ZUtils::serialize_to_socket(zock,
                                     static_cast<vfsprotocol::RequestType>(req_type),
                                     MoreMessageParts::T);

    const Object obj(ObjectType::Volume,
                          vname);

    const auto rreq(vfsprotocol::MessageUtils::create_read_request(obj,
                                                                   pattern.size(),
                                                                   off));

    vfsprotocol::Tag tag(reinterpret_cast<uint64_t>(&rreq));

    ZUtils::serialize_to_socket(zock,
                                     tag,
                                     MoreMessageParts::T);

    ZUtils::serialize_to_socket(zock, rreq, MoreMessageParts::F);

    vfsprotocol::ResponseType rsp;
    ZUtils::deserialize_from_socket(zock, rsp);

    EXPECT_TRUE(rsp == vfsprotocol::ResponseType::UnknownRequest);

    ZEXPECT_MORE(zock, "tag");

    vfsprotocol::Tag rsp_tag;
    ZUtils::deserialize_from_socket(zock, rsp_tag);

    EXPECT_EQ(tag, rsp_tag);

    ZEXPECT_NOTHING_MORE(zock);

    // we should still be able to read afterwards
    EXPECT_EQ(pattern, remote_read(vname, pattern.size(), off));
}

TEST_F(ObjectRouterTest, wrong_request_type)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = get_cluster_size(vname) - 1;

    write_to_file(fname, pattern, pattern.size(), off);

    ZUtils::serialize_to_socket(zock,
                                     vfsprotocol::RequestType::Write,
                                     MoreMessageParts::T);

    const Object obj(ObjectType::Volume,
                          vname);

    const auto rreq(vfsprotocol::MessageUtils::create_read_request(obj,
                                                                   pattern.size(),
                                                                   off));

    vfsprotocol::Tag tag(reinterpret_cast<uint64_t>(&rreq));

    ZUtils::serialize_to_socket(zock,
                                     tag,
                                     MoreMessageParts::T);

    ZUtils::serialize_to_socket(zock, rreq, MoreMessageParts::F);

    vfsprotocol::ResponseType rsp;
    ZUtils::deserialize_from_socket(zock, rsp);

    EXPECT_TRUE(rsp == vfsprotocol::ResponseType::ProtocolError);

    ZEXPECT_MORE(zock, "tag");

    vfsprotocol::Tag rsp_tag;
    ZUtils::deserialize_from_socket(zock, rsp_tag);

    EXPECT_EQ(tag, rsp_tag);

    ZEXPECT_NOTHING_MORE(zock);

    // we should still be able to read afterwards
    EXPECT_EQ(pattern, remote_read(vname, pattern.size(), off));
}

TEST_F(ObjectRouterTest, missing_tag)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = get_cluster_size(vname) - 1;

    write_to_file(fname, pattern, pattern.size(), off);

    ZUtils::serialize_to_socket(zock,
                                     vfsprotocol::RequestType::Write,
                                     MoreMessageParts::T);

    const Object obj(ObjectType::Volume,
                          vname);

    const auto rreq(vfsprotocol::MessageUtils::create_read_request(obj,
                                                                   pattern.size(),
                                                                   off));

    ZUtils::serialize_to_socket(zock, rreq, MoreMessageParts::F);

    vfsprotocol::ResponseType rsp;
    ZUtils::deserialize_from_socket(zock, rsp);

    EXPECT_TRUE(rsp == vfsprotocol::ResponseType::ProtocolError);

    ZEXPECT_MORE(zock, "tag");

    vfsprotocol::Tag rsp_tag;
    ZUtils::deserialize_from_socket(zock, rsp_tag);

    ZEXPECT_NOTHING_MORE(zock);

    // we should still be able to read afterwards
    EXPECT_EQ(pattern, remote_read(vname, pattern.size(), off));
}

TEST_F(ObjectRouterTest, config_and_node_maps)
{
    const ObjectRouter& router = fs_->object_router();
    ConfigMap cm(config_map(router));
    EXPECT_EQ(2U,
              cm.size());

    NodeMap nm(node_map(router));
    EXPECT_EQ(2U,
              nm.size());

    std::shared_ptr<ClusterNode> local(nm[local_config().vrouter_id]);
    ASSERT_TRUE(local != nullptr);
    EXPECT_TRUE(std::dynamic_pointer_cast<LocalNode>(local) != nullptr);

    {
        auto it = cm.find(local_config().vrouter_id);
        ASSERT_TRUE(it != cm.end());
        EXPECT_EQ(local_config(),
                  it->second);
        EXPECT_EQ(local_config().message_uri(),
                  local->uri());
    }

    std::shared_ptr<ClusterNode> remote(nm[remote_config().vrouter_id]);
    ASSERT_TRUE(remote != nullptr);
    EXPECT_TRUE(std::dynamic_pointer_cast<RemoteNode>(remote) != nullptr);

    {
        auto it = cm.find(remote_config().vrouter_id);
        ASSERT_TRUE(it != cm.end());
        EXPECT_EQ(remote_config(),
                  it->second);
        EXPECT_EQ(remote_config().message_uri(),
                  remote->uri());
    }
}

TEST_F(ObjectRouterTest, expand_cluster)
{
    ObjectRouter& router = fs_->object_router();
    std::shared_ptr<ClusterRegistry> registry(router.cluster_registry());

    {
        const NodeMap nm(node_map(router));
        const ConfigMap cm(config_map(router));

        ASSERT_FALSE(nm.empty());
        ASSERT_EQ(2U,
                  nm.size());
        ASSERT_EQ(2U,
                  cm.size());

        ASSERT_TRUE(nm.find(local_config().vrouter_id) != nm.end());
        ASSERT_TRUE(nm.find(remote_config().vrouter_id) != nm.end());
        ASSERT_TRUE(cm.find(local_config().vrouter_id) != cm.end());
        ASSERT_TRUE(cm.find(remote_config().vrouter_id) != cm.end());
    }

    const NodeId new_node_id(yt::UUID().str());
    const ClusterNodeConfig new_config(expand_cluster(router,
                                                           new_node_id));

    {
        const NodeMap nm(node_map(router));
        const ConfigMap cm(config_map(router));

        EXPECT_FALSE(nm.empty());
        EXPECT_EQ(3U,
                  nm.size());
        EXPECT_EQ(3U,
                  cm.size());

        EXPECT_TRUE(nm.find(local_config().vrouter_id) != nm.end());
        EXPECT_TRUE(nm.find(remote_config().vrouter_id) != nm.end());
        EXPECT_TRUE(cm.find(local_config().vrouter_id) != cm.end());
        EXPECT_TRUE(cm.find(remote_config().vrouter_id) != cm.end());

        {
            auto it = nm.find(new_node_id);
            ASSERT_TRUE(it != nm.end());
            const std::shared_ptr<ClusterNode> new_node(it->second);
            ASSERT_TRUE(new_node != nullptr);
            EXPECT_EQ(new_config.message_uri(),
                      new_node->uri());
        }

        {
            auto it = cm.find(new_node_id);
            ASSERT_TRUE(it != cm.end());
            EXPECT_EQ(new_config,
                      it->second);
        }
    }
}

TEST_F(ObjectRouterTest, shrink_cluster)
{
    ObjectRouter& router = fs_->object_router();
    std::shared_ptr<ClusterRegistry> registry(router.cluster_registry());

    EXPECT_NE(boost::none,
              router.node_config(remote_config().vrouter_id));

    {
        const NodeMap nm(node_map(router));
        const ConfigMap cm(config_map(router));

        EXPECT_EQ(2U,
                  nm.size());

        EXPECT_TRUE(nm.find(local_config().vrouter_id) != nm.end());
        EXPECT_TRUE(nm.find(remote_config().vrouter_id) != nm.end());
        EXPECT_TRUE(cm.find(local_config().vrouter_id) != cm.end());
        EXPECT_TRUE(cm.find(remote_config().vrouter_id) != cm.end());
    }

    registry->erase_node_configs();
    registry->set_node_configs({ router.node_config() });

    router.update_cluster_node_configs();

    EXPECT_EQ(boost::none,
              router.node_config(remote_config().vrouter_id));

    {
        const NodeMap nm(node_map(router));
        const ConfigMap cm(config_map(router));

        EXPECT_EQ(1U,
                  nm.size());
        EXPECT_EQ(1U,
                  cm.size());

        EXPECT_TRUE(nm.find(local_config().vrouter_id) != nm.end());
        EXPECT_TRUE(nm.find(remote_config().vrouter_id) == nm.end());
        EXPECT_TRUE(cm.find(local_config().vrouter_id) != cm.end());
        EXPECT_TRUE(cm.find(remote_config().vrouter_id) == cm.end());
    }
}

TEST_F(ObjectRouterTest, enforce_local_node)
{
    ObjectRouter& router = fs_->object_router();
    std::shared_ptr<ClusterRegistry> registry(router.cluster_registry());

    EXPECT_NE(boost::none,
              router.node_config(remote_config().vrouter_id));

    auto check([&]
               {
                   const NodeMap nm(node_map(router));
                   const ConfigMap cm(config_map(router));

                   EXPECT_EQ(2U,
                             nm.size());
                   EXPECT_EQ(2U,
                             cm.size());

                   EXPECT_TRUE(nm.find(local_config().vrouter_id) != nm.end());
                   EXPECT_TRUE(nm.find(remote_config().vrouter_id) != nm.end());
                   EXPECT_TRUE(cm.find(local_config().vrouter_id) != cm.end());
                   EXPECT_TRUE(cm.find(remote_config().vrouter_id) != cm.end());
               });

    check();

    const ClusterNodeConfig
        rconfig(*router.node_config(remote_config().vrouter_id));

    registry->erase_node_configs();
    registry->set_node_configs({ rconfig });

    EXPECT_THROW(router.update_cluster_node_configs(),
                 InvalidConfigurationException);

    EXPECT_NE(boost::none,
              router.node_config(remote_config().vrouter_id));

    check();
}

TEST_F(ObjectRouterTest, no_modification_of_existing_node_config)
{
    ObjectRouter& router = fs_->object_router();
    std::shared_ptr<ClusterRegistry> registry(router.cluster_registry());

    auto check([&]
               {
                   const NodeMap nm(node_map(router));
                   const ConfigMap cm(config_map(router));

                   EXPECT_EQ(2U,
                             nm.size());
                   EXPECT_EQ(2U,
                             cm.size());

                   EXPECT_TRUE(nm.find(local_config().vrouter_id) != nm.end());
                   EXPECT_TRUE(nm.find(remote_config().vrouter_id) != nm.end());
                   EXPECT_TRUE(cm.find(local_config().vrouter_id) != cm.end());
                   EXPECT_TRUE(cm.find(remote_config().vrouter_id) != cm.end());
               });

    check();

    const ClusterNodeConfigs configs{ local_config(),
            ClusterNodeConfig(remote_config().vrouter_id,
                                   remote_config().message_host,
                                   MessagePort(remote_config().message_port + 1),
                                   remote_config().xmlrpc_port,
                                   remote_config().failovercache_port,
                                   remote_config().network_server_uri,
                                   remote_config().xmlrpc_host,
                                   remote_config().failovercache_host), };

    registry->erase_node_configs();
    registry->set_node_configs(configs);

    EXPECT_THROW(router.update_cluster_node_configs(),
                 InvalidConfigurationException);

    check();
}

TEST_F(ObjectRouterTest, allow_node_distance_map_updates)
{
    ObjectRouter& router = fs_->object_router();
    std::shared_ptr<ClusterRegistry> registry(router.cluster_registry());

    auto check([&](const boost::optional<ClusterNodeConfig::NodeDistanceMap>& exp_dist_map)
               {
                   const NodeMap nm(node_map(router));
                   const ConfigMap cm(config_map(router));

                   EXPECT_EQ(2U,
                             nm.size());
                   EXPECT_EQ(2U,
                             cm.size());

                   EXPECT_TRUE(nm.find(local_config().vrouter_id) != nm.end());
                   EXPECT_TRUE(nm.find(remote_config().vrouter_id) != nm.end());

                   {
                       auto it = cm.find(local_config().vrouter_id);
                       ASSERT_TRUE(it != cm.end());
                       EXPECT_TRUE(exp_dist_map == it->second.node_distance_map);
                   }
                   {
                       auto it = cm.find(remote_config().vrouter_id);
                       ASSERT_TRUE(it != cm.end());
                       EXPECT_TRUE(exp_dist_map == it->second.node_distance_map);
                   }
               });

    check(boost::none);

    const ClusterNodeConfig::NodeDistanceMap ndm;
    ClusterNodeConfig local(local_config());
    local.node_distance_map = ndm;
    ClusterNodeConfig remote(remote_config());
    remote.node_distance_map = ndm;

    registry->erase_node_configs();
    registry->set_node_configs({ local, remote });
    router.update_cluster_node_configs();

    check(ndm);
}

TEST_F(ObjectRouterTest, volume_snapshot_create_rollback_delete)
{
    const FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const ObjectId vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = get_cluster_size(vname) - 1;

    write_to_file(fname, pattern, pattern.size(), off);

    const vd::SnapshotName snap("some-volume-snap1");
    EXPECT_NO_THROW(fs_->object_router().create_snapshot(vname,
                                                         snap,
                                                         0));
    wait_for_snapshot(vname, "some-volume-snap1", 30);

    std::vector<std::string> snaps;
    snaps = fs_->object_router().list_snapshots(vname);

    EXPECT_EQ(1U,
              snaps.size());

    EXPECT_THROW(fs_->object_router().create_snapshot(vname,
                                                      snap,
                                                      0),
                 clienterrors::SnapshotNameAlreadyExistsException);
    EXPECT_NO_THROW(fs_->object_router().delete_snapshot(vname,
                                                         snap));

    snaps = fs_->object_router().list_snapshots(vname);

    EXPECT_EQ(0U,
              snaps.size());

    EXPECT_THROW(fs_->object_router().delete_snapshot(vname,
                                                      snap),
                 clienterrors::SnapshotNotFoundException);
    EXPECT_THROW(fs_->object_router().rollback_volume(vname,
                                                      snap),
                 clienterrors::SnapshotNotFoundException);

    const ObjectId fvname("/f-volume");
    EXPECT_THROW(fs_->object_router().create_snapshot(fvname,
                                                      snap,
                                                      0),
                 clienterrors::ObjectNotFoundException);

    EXPECT_THROW(snaps = fs_->object_router().list_snapshots(fvname),
                 clienterrors::ObjectNotFoundException);
}

}
