// Copyright 2015 Open vStorage NV
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

    zmq::context_t ztx;
    zmq::socket_t zock;
};

TEST_F(ObjectRouterTest, remote_read)
{
    const vfs::FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = api::GetClusterSize() - 1;

    write_to_file(fname, pattern, pattern.size(), off);
    EXPECT_EQ(pattern, remote_read(vname, pattern.size(), off));
}

TEST_F(ObjectRouterTest, remote_write)
{
    const vfs::FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("remotely written");
    const uint64_t off = api::GetClusterSize() - 1;

    remote_write(vname, pattern, off);
    check_file(fname, pattern, pattern.size(), off);
}

TEST_F(ObjectRouterTest, invalid_request_type)
{
    const vfs::FrontendPath fname(make_volume_name("/some-volume"));
    const uint64_t vsize = 10 << 20;
    const auto vname(create_file(fname, vsize));

    const std::string pattern("locally written");
    const uint64_t off = api::GetClusterSize() - 1;

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
    const uint64_t off = api::GetClusterSize() - 1;

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
    const uint64_t off = api::GetClusterSize() - 1;

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

    vfs::ClusterNodeConfigs configs(registry->get_node_configs());
    const vfs::NodeId new_node_id(yt::UUID().str());
    const vfs::ClusterNodeConfig
            new_config(new_node_id,
                       local_config().host,
                       vfs::MessagePort(local_config().message_port + 2),
                       vfs::XmlRpcPort(local_config().xmlrpc_port + 2),
                       vfs::FailoverCachePort(local_config().failovercache_port + 2));

    configs.push_back(new_config);

    registry->erase_node_configs();
    registry->set_node_configs(configs);

    router.update_cluster_node_configs();

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
                                   remote_config().failovercache_port) };

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

}
