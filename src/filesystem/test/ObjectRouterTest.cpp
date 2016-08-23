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
namespace vfs = volumedriverfs;
namespace yt = youtils;

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
                               local_config().host +
                               ":"s +
                               boost::lexical_cast<std::string>(local_config().message_port));
        zock.connect(addr.c_str());
    }

    // Do we really want to assemble the message parts here?
    // (Try to) unify with the production client side code instead.
    std::string
    remote_read(const vfs::ObjectId& vname,
                uint64_t size,
                uint64_t off)
    {
        const vfs::Object obj(vfs::ObjectType::Volume,
                              vname);

        const auto rreq(vfsprotocol::MessageUtils::create_read_request(obj,
                                                                       size,
                                                                       off));
        vfs::ZUtils::serialize_to_socket(zock,
                                         vfsprotocol::RequestType::Read,
                                         vfs::MoreMessageParts::T);

        vfsprotocol::Tag tag(reinterpret_cast<uint64_t>(&rreq));

        vfs::ZUtils::serialize_to_socket(zock,
                                         tag,
                                         vfs::MoreMessageParts::T);

        vfs::ZUtils::serialize_to_socket(zock, rreq, vfs::MoreMessageParts::F);

        vfsprotocol::ResponseType rsp;
        vfs::ZUtils::deserialize_from_socket(zock, rsp);

        EXPECT_TRUE(rsp == vfsprotocol::ResponseType::Ok);

        ZEXPECT_MORE(zock, "tag");

        vfsprotocol::Tag rsp_tag;
        vfs::ZUtils::deserialize_from_socket(zock, rsp_tag);
        EXPECT_EQ(tag, rsp_tag);

        ZEXPECT_MORE(zock, "read data");

        zmq::message_t read_data;
        zock.recv(&read_data);

        EXPECT_EQ(size, read_data.size());
        const std::string s(static_cast<const char*>(read_data.data()), read_data.size());

        return s;
    }

    void
    remote_write(const vfs::ObjectId& vname,
                 const std::string& pattern,
                 uint64_t off)
    {
        const vfs::Object obj(vfs::ObjectType::Volume,
                              vname);

        const auto wreq(vfsprotocol::MessageUtils::create_write_request(obj,
                                                                        pattern.size(),
                                                                        off));

        vfs::ZUtils::serialize_to_socket(zock,
                                         vfsprotocol::RequestType::Write,
                                         vfs::MoreMessageParts::T);

        const vfsprotocol::Tag tag(reinterpret_cast<uint64_t>(&wreq));
        vfs::ZUtils::serialize_to_socket(zock,
                                         tag,
                                         vfs::MoreMessageParts::T);

        vfs::ZUtils::serialize_to_socket(zock,
                                         wreq,
                                         vfs::MoreMessageParts::T);

        zmq::message_t write_data(pattern.size());
        memcpy(write_data.data(), pattern.c_str(), pattern.size());
        zock.send(write_data, 0);

        vfsprotocol::ResponseType rsp;
        vfs::ZUtils::deserialize_from_socket(zock, rsp);

        EXPECT_TRUE(rsp == vfsprotocol::ResponseType::Ok);

        ZEXPECT_MORE(zock, "tag");

        vfsprotocol::Tag rsp_tag;
        vfs::ZUtils::deserialize_from_socket(zock, rsp_tag);

        EXPECT_EQ(tag, rsp_tag);

        ZEXPECT_MORE(zock, "WriteResponse");

        vfsprotocol::WriteResponse wrsp;
        vfs::ZUtils::deserialize_from_socket(zock, wrsp);

        EXPECT_EQ(pattern.size(), wrsp.size());

        ZEXPECT_NOTHING_MORE(zock);
    }

    using NodeMap = vfs::ObjectRouter::NodeMap;

    static const NodeMap&
    node_map(const vfs::ObjectRouter& r)
    {
        return r.node_map_;
    }

    NodeMap
    build_node_map(vfs::ObjectRouter& r)
    {
        return r.build_node_map_(boost::none);
    }

    vfs::ClusterNodeConfig
    expand_cluster(vfs::ObjectRouter& router,
                   const vfs::NodeId& new_node_id)
    {
        std::shared_ptr<vfs::ClusterRegistry> registry(router.cluster_registry());

        vfs::ClusterNodeConfigs configs(registry->get_node_configs());

        const vfs::ClusterNodeConfig
            new_config(new_node_id,
                       local_config().host,
                       vfs::MessagePort(local_config().message_port + next_port_off_),
                       vfs::XmlRpcPort(local_config().xmlrpc_port + next_port_off_),
                       vfs::FailoverCachePort(local_config().failovercache_port + next_port_off_),
                       make_edge_uri_(local_node_id()));

        ++next_port_off_;

        configs.push_back(new_config);

        registry->erase_node_configs();
        registry->set_node_configs(configs);

        router.update_cluster_node_configs();

        return new_config;
    }

    bool
    steal(vfs::ObjectRouter& router,
          const vfs::ObjectRegistration& reg,
          const vfs::OnlyStealFromOfflineNode only_steal_from_offline_node)
    {
        return router.steal_(reg,
                             only_steal_from_offline_node);
    }

    zmq::context_t ztx;
    zmq::socket_t zock;
    uint16_t next_port_off_;
};

TEST_F(ObjectRouterTest, remote_read)
{
    const vfs::FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = get_cluster_size(vname) - 1;

    write_to_file(fname, pattern, pattern.size(), off);
    EXPECT_EQ(pattern, remote_read(vname, pattern.size(), off));
}

TEST_F(ObjectRouterTest, remote_write)
{
    const vfs::FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("remotely written");
    const uint64_t off = get_cluster_size(vname) - 1;

    remote_write(vname, pattern, off);
    check_file(fname, pattern, pattern.size(), off);
}

TEST_F(ObjectRouterTest, invalid_request_type)
{
    const vfs::FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = get_cluster_size(vname) - 1;

    write_to_file(fname, pattern, pattern.size(), off);

    const uint32_t req_type = 0;
    static_assert(sizeof(req_type) == sizeof(vfsprotocol::RequestType),
                  "fix yer test");

    vfs::ZUtils::serialize_to_socket(zock,
                                     static_cast<vfsprotocol::RequestType>(req_type),
                                     vfs::MoreMessageParts::T);

    const vfs::Object obj(vfs::ObjectType::Volume,
                          vname);

    const auto rreq(vfsprotocol::MessageUtils::create_read_request(obj,
                                                                   pattern.size(),
                                                                   off));

    vfsprotocol::Tag tag(reinterpret_cast<uint64_t>(&rreq));

    vfs::ZUtils::serialize_to_socket(zock,
                                     tag,
                                     vfs::MoreMessageParts::T);

    vfs::ZUtils::serialize_to_socket(zock, rreq, vfs::MoreMessageParts::F);

    vfsprotocol::ResponseType rsp;
    vfs::ZUtils::deserialize_from_socket(zock, rsp);

    EXPECT_TRUE(rsp == vfsprotocol::ResponseType::UnknownRequest);

    ZEXPECT_MORE(zock, "tag");

    vfsprotocol::Tag rsp_tag;
    vfs::ZUtils::deserialize_from_socket(zock, rsp_tag);

    EXPECT_EQ(tag, rsp_tag);

    ZEXPECT_NOTHING_MORE(zock);

    // we should still be able to read afterwards
    EXPECT_EQ(pattern, remote_read(vname, pattern.size(), off));
}

TEST_F(ObjectRouterTest, wrong_request_type)
{
    const vfs::FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = get_cluster_size(vname) - 1;

    write_to_file(fname, pattern, pattern.size(), off);

    vfs::ZUtils::serialize_to_socket(zock,
                                     vfsprotocol::RequestType::Write,
                                     vfs::MoreMessageParts::T);

    const vfs::Object obj(vfs::ObjectType::Volume,
                          vname);

    const auto rreq(vfsprotocol::MessageUtils::create_read_request(obj,
                                                                   pattern.size(),
                                                                   off));

    vfsprotocol::Tag tag(reinterpret_cast<uint64_t>(&rreq));

    vfs::ZUtils::serialize_to_socket(zock,
                                     tag,
                                     vfs::MoreMessageParts::T);

    vfs::ZUtils::serialize_to_socket(zock, rreq, vfs::MoreMessageParts::F);

    vfsprotocol::ResponseType rsp;
    vfs::ZUtils::deserialize_from_socket(zock, rsp);

    EXPECT_TRUE(rsp == vfsprotocol::ResponseType::ProtocolError);

    ZEXPECT_MORE(zock, "tag");

    vfsprotocol::Tag rsp_tag;
    vfs::ZUtils::deserialize_from_socket(zock, rsp_tag);

    EXPECT_EQ(tag, rsp_tag);

    ZEXPECT_NOTHING_MORE(zock);

    // we should still be able to read afterwards
    EXPECT_EQ(pattern, remote_read(vname, pattern.size(), off));
}

TEST_F(ObjectRouterTest, missing_tag)
{
    const vfs::FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = get_cluster_size(vname) - 1;

    write_to_file(fname, pattern, pattern.size(), off);

    vfs::ZUtils::serialize_to_socket(zock,
                                     vfsprotocol::RequestType::Write,
                                     vfs::MoreMessageParts::T);

    const vfs::Object obj(vfs::ObjectType::Volume,
                          vname);

    const auto rreq(vfsprotocol::MessageUtils::create_read_request(obj,
                                                                   pattern.size(),
                                                                   off));

    vfs::ZUtils::serialize_to_socket(zock, rreq, vfs::MoreMessageParts::F);

    vfsprotocol::ResponseType rsp;
    vfs::ZUtils::deserialize_from_socket(zock, rsp);

    EXPECT_TRUE(rsp == vfsprotocol::ResponseType::ProtocolError);

    ZEXPECT_MORE(zock, "tag");

    vfsprotocol::Tag rsp_tag;
    vfs::ZUtils::deserialize_from_socket(zock, rsp_tag);

    ZEXPECT_NOTHING_MORE(zock);

    // we should still be able to read afterwards
    EXPECT_EQ(pattern, remote_read(vname, pattern.size(), off));
}

TEST_F(ObjectRouterTest, node_map)
{
    const vfs::ObjectRouter& router = fs_->object_router();
    NodeMap nm(node_map(router));

    EXPECT_EQ(2U,
              nm.size());

    std::shared_ptr<vfs::ClusterNode> local(nm[local_config().vrouter_id]);
    ASSERT_TRUE(local != nullptr);
    EXPECT_TRUE(std::dynamic_pointer_cast<vfs::LocalNode>(local) != nullptr);
    EXPECT_EQ(local_config(),
              local->config);

    std::shared_ptr<vfs::ClusterNode> remote(nm[remote_config().vrouter_id]);
    ASSERT_TRUE(remote != nullptr);
    EXPECT_TRUE(std::dynamic_pointer_cast<vfs::RemoteNode>(remote) != nullptr);
    EXPECT_EQ(remote_config(),
              remote->config);
}

TEST_F(ObjectRouterTest, expand_cluster)
{
    vfs::ObjectRouter& router = fs_->object_router();
    std::shared_ptr<vfs::ClusterRegistry> registry(router.cluster_registry());

    {
        NodeMap nm(node_map(router));

        ASSERT_FALSE(nm.empty());
        ASSERT_EQ(2U,
                  nm.size());

        ASSERT_TRUE(nm[local_config().vrouter_id] != nullptr);
        ASSERT_TRUE(nm[remote_config().vrouter_id] != nullptr);
    }

    const vfs::NodeId new_node_id(yt::UUID().str());
    const vfs::ClusterNodeConfig new_config(expand_cluster(router,
                                                           new_node_id));

    {
        NodeMap nm(node_map(router));

        EXPECT_FALSE(nm.empty());
        EXPECT_EQ(3U,
                  nm.size());

        EXPECT_TRUE(nm[local_config().vrouter_id] != nullptr);
        EXPECT_TRUE(nm[remote_config().vrouter_id] != nullptr);

        const std::shared_ptr<vfs::ClusterNode> new_node(nm[new_node_id]);
        ASSERT_TRUE(new_node != nullptr);
        EXPECT_EQ(new_config,
                  new_node->config);
    }
}

TEST_F(ObjectRouterTest, shrink_cluster)
{
    vfs::ObjectRouter& router = fs_->object_router();
    std::shared_ptr<vfs::ClusterRegistry> registry(router.cluster_registry());

    EXPECT_NE(boost::none,
              router.node_config(remote_config().vrouter_id));

    {
        NodeMap nm(node_map(router));

        EXPECT_EQ(2U,
                  nm.size());

        EXPECT_TRUE(nm[local_config().vrouter_id] != nullptr);
        EXPECT_TRUE(nm[remote_config().vrouter_id] != nullptr);
    }

    registry->erase_node_configs();
    registry->set_node_configs({ router.node_config() });

    router.update_cluster_node_configs();

    EXPECT_EQ(boost::none,
              router.node_config(remote_config().vrouter_id));

    {
        NodeMap nm(node_map(router));

        EXPECT_EQ(1U,
                  nm.size());

        EXPECT_TRUE(nm[local_config().vrouter_id] != nullptr);
        EXPECT_TRUE(nm[remote_config().vrouter_id] == nullptr);
    }
}

TEST_F(ObjectRouterTest, enforce_local_node)
{
    vfs::ObjectRouter& router = fs_->object_router();
    std::shared_ptr<vfs::ClusterRegistry> registry(router.cluster_registry());

    EXPECT_NE(boost::none,
              router.node_config(remote_config().vrouter_id));

    {
        NodeMap nm(node_map(router));

        EXPECT_EQ(2U,
                  nm.size());

        EXPECT_TRUE(nm[local_config().vrouter_id] != nullptr);
        EXPECT_TRUE(nm[remote_config().vrouter_id] != nullptr);
    }

    const vfs::ClusterNodeConfig
        rconfig(*router.node_config(remote_config().vrouter_id));

    registry->erase_node_configs();
    registry->set_node_configs({ rconfig });

    EXPECT_THROW(router.update_cluster_node_configs(),
                 vfs::InvalidConfigurationException);

    EXPECT_NE(boost::none,
              router.node_config(remote_config().vrouter_id));

    {
        NodeMap nm(node_map(router));

        EXPECT_EQ(2U,
                  nm.size());

        EXPECT_TRUE(nm[local_config().vrouter_id] != nullptr);
        EXPECT_TRUE(nm[remote_config().vrouter_id] != nullptr);
    }
}

TEST_F(ObjectRouterTest, no_modification_of_existing_node_config)
{
    vfs::ObjectRouter& router = fs_->object_router();
    std::shared_ptr<vfs::ClusterRegistry> registry(router.cluster_registry());

    {
        NodeMap nm(node_map(router));

        EXPECT_EQ(2U,
                  nm.size());

        EXPECT_TRUE(nm[local_config().vrouter_id] != nullptr);
        EXPECT_TRUE(nm[remote_config().vrouter_id] != nullptr);
    }

    const vfs::ClusterNodeConfigs configs{ local_config(),
            vfs::ClusterNodeConfig(remote_config().vrouter_id,
                                   remote_config().host,
                                   vfs::MessagePort(remote_config().message_port + 1),
                                   remote_config().xmlrpc_port,
                                   remote_config().failovercache_port,
                                   remote_config().network_server_uri) };

    registry->erase_node_configs();
    registry->set_node_configs(configs);

    EXPECT_THROW(router.update_cluster_node_configs(),
                 vfs::InvalidConfigurationException);

    {
        NodeMap nm(node_map(router));

        EXPECT_EQ(2U,
                  nm.size());

        EXPECT_TRUE(nm[local_config().vrouter_id] != nullptr);

        const std::shared_ptr<vfs::ClusterNode>
            remote(nm[remote_config().vrouter_id]);
        ASSERT_TRUE(remote != nullptr);
        EXPECT_EQ(remote_config(),
                  remote->config);
    }
}

TEST_F(ObjectRouterTest, volume_snapshot_create_rollback_delete)
{
    const vfs::FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const vfs::ObjectId vname(create_file(fname, vsize));

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
                 vfs::clienterrors::SnapshotNameAlreadyExistsException);
    EXPECT_NO_THROW(fs_->object_router().delete_snapshot(vname,
                                                         snap));

    snaps = fs_->object_router().list_snapshots(vname);

    EXPECT_EQ(0U,
              snaps.size());

    EXPECT_THROW(fs_->object_router().delete_snapshot(vname,
                                                      snap),
                 vfs::clienterrors::SnapshotNotFoundException);
    EXPECT_THROW(fs_->object_router().rollback_volume(vname,
                                                      snap),
                 vfs::clienterrors::SnapshotNotFoundException);

    const vfs::ObjectId fvname("/f-volume");
    EXPECT_THROW(fs_->object_router().create_snapshot(fvname,
                                                      snap,
                                                      0),
                 vfs::clienterrors::ObjectNotFoundException);

    EXPECT_THROW(snaps = fs_->object_router().list_snapshots(fvname),
                 vfs::clienterrors::ObjectNotFoundException);
}

}
