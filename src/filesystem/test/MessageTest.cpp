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

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <gtest/gtest.h>

#include <volumedriver/Types.h>

#include "../MessageUtils.h"

namespace volumedriverfstest
{

namespace vd = volumedriver;
namespace vfs = volumedriverfs;

class MessageTest
    : public testing::Test
{
protected:
    template<typename M>
    void
    check_object_type(const M& msg,
                      vfs::ObjectType exp)
    {
        EXPECT_TRUE(exp == static_cast<vfs::ObjectType>(msg.object_type()));
    }
};

TEST_F(MessageTest, missing_required_field)
{
    const vfsprotocol::GetSizeRequest msg;

    // object_id is mandatory.
    ASSERT_FALSE(msg.has_object_id());

#ifndef NDEBUG
    EXPECT_THROW(msg.SerializeAsString(),
                 std::exception);
#endif //NDEBUG

    EXPECT_THROW(msg.CheckInitialized(),
                 std::exception);
}

TEST_F(MessageTest, write_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::File;
    const uint64_t size = 1;
    const uint64_t offset = 2;

    const auto msg(vfsprotocol::MessageUtils::create_write_request(vfs::Object(tp, id),
                                                                   size,
                                                                   offset));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::WriteRequest msg2;
    msg2.ParseFromString(s);
    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    check_object_type(msg2, tp);
    EXPECT_EQ(size, msg2.size());
    EXPECT_EQ(offset, msg2.offset());
}

TEST_F(MessageTest, read_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::Volume;
    const uint64_t size = 1;
    const uint64_t offset = 2;

    const auto msg(vfsprotocol::MessageUtils::create_read_request(vfs::Object(tp, id),
                                                                  size,
                                                                  offset));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::ReadRequest msg2;
    msg2.ParseFromString(s);
    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    check_object_type(msg2, tp);
    EXPECT_EQ(size, msg2.size());
    EXPECT_EQ(offset, msg2.offset());
}

TEST_F(MessageTest, resize_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::Volume;

    const uint64_t size = 1;
    const auto msg(vfsprotocol::MessageUtils::create_resize_request(vfs::Object(tp, id),
                                                                    size));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::ResizeRequest msg2;
    msg2.ParseFromString(s);
    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    check_object_type(msg2, tp);
    EXPECT_EQ(size, msg2.size());
}

TEST_F(MessageTest, sync_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::File;

    const auto msg(vfsprotocol::MessageUtils::create_sync_request(vfs::Object(tp, id)));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::SyncRequest msg2;
    msg2.ParseFromString(s);
    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    check_object_type(msg2, tp);
}

TEST_F(MessageTest, get_size_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::File;

    const auto msg(vfsprotocol::MessageUtils::create_get_size_request(vfs::Object(tp, id)));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::GetSizeRequest msg2;
    msg2.ParseFromString(s);
    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    check_object_type(msg2, tp);
}

TEST_F(MessageTest, get_size_response)
{
    const uint64_t size = 42;
    const auto msg(vfsprotocol::MessageUtils::create_get_size_response(size));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::GetSizeResponse msg2;
    msg2.ParseFromString(s);

    ASSERT_TRUE(msg2.IsInitialized());
    EXPECT_EQ(size, msg2.size());
}

TEST_F(MessageTest, get_cluster_multiplier_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::File;

    const auto msg(vfsprotocol::MessageUtils::create_get_cluster_multiplier_request(vfs::Object(tp, id)));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::GetClusterMultiplierRequest msg2;
    msg2.ParseFromString(s);
    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    check_object_type(msg2, tp);
}

TEST_F(MessageTest, get_cluster_multiplier_response)
{
    const vd::ClusterMultiplier cm(4096);
    const auto msg(vfsprotocol::MessageUtils::create_get_cluster_multiplier_response(cm));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::GetClusterMultiplierResponse msg2;
    msg2.ParseFromString(s);

    ASSERT_TRUE(msg2.IsInitialized());
    EXPECT_EQ(static_cast<uint32_t>(cm), msg2.size());
}

TEST_F(MessageTest, get_clone_namespace_map_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::File;

    const auto msg(vfsprotocol::MessageUtils::create_get_clone_namespace_map_request(vfs::Object(tp, id)));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::GetCloneNamespaceMapRequest msg2;
    msg2.ParseFromString(s);
    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    check_object_type(msg2, tp);
}

TEST_F(MessageTest, get_clone_namespace_map_response)
{
    vd::CloneNamespaceMap cnm;
    auto clone_id_0 = vd::SCOCloneID(88);
    auto ns0 = backend::Namespace(std::string("namespace0"));
    cnm.emplace(clone_id_0, ns0);

    auto clone_id_1 = vd::SCOCloneID(89);
    auto ns1 = backend::Namespace(std::string("namespace1"));
    cnm.emplace(clone_id_1, ns1);

    const auto msg(vfsprotocol::MessageUtils::create_get_clone_namespace_map_response(cnm));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::GetCloneNamespaceMapResponse msg2;
    msg2.ParseFromString(s);

    ASSERT_TRUE(msg2.IsInitialized());
    EXPECT_EQ(2UL, msg2.map_entry_size());

    vd::CloneNamespaceMap tmp;
    for (int i = 0; i < msg2.map_entry_size(); i++)
    {
        const auto& e = msg2.map_entry(i);
        tmp.emplace(vd::SCOCloneID(e.clone_id()), backend::Namespace(e.ns()));
    }
    EXPECT_TRUE(tmp.find(clone_id_0) != tmp.end());
    EXPECT_EQ(ns0, tmp.find(clone_id_0)->second);

    EXPECT_TRUE(tmp.find(clone_id_1) != tmp.end());
    EXPECT_EQ(ns1, tmp.find(clone_id_1)->second);
}

TEST_F(MessageTest, get_page_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::File;
    const vd::ClusterAddress ca(888);

    const auto msg(vfsprotocol::MessageUtils::create_get_page_request(vfs::Object(tp, id),
                                                                      ca));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::GetPageRequest msg2;
    msg2.ParseFromString(s);
    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    EXPECT_EQ(ca, vd::ClusterAddress(msg2.cluster_address()));
    check_object_type(msg2, tp);
}

TEST_F(MessageTest, get_page_response)
{
    std::vector<vd::ClusterLocationAndHash> clp;
    const vd::ClusterLocationAndHash clh_0(vd::ClusterLocation(vd::SCONumber(0),
                                                               vd::SCOOffset(88),
                                                               vd::SCOCloneID(89),
                                                               vd::SCOVersion(90)));
    const vd::ClusterLocationAndHash clh_1(vd::ClusterLocation(vd::SCONumber(1),
                                                               vd::SCOOffset(91),
                                                               vd::SCOCloneID(92),
                                                               vd::SCOVersion(93)));
    clp.push_back(clh_0);
    clp.push_back(clh_1);

    EXPECT_TRUE(clp[0] == clh_0);
    EXPECT_TRUE(clp[1] == clh_1);

    const auto msg(vfsprotocol::MessageUtils::create_get_page_response(clp));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::GetPageResponse msg2;
    msg2.ParseFromString(s);

    ASSERT_TRUE(msg2.IsInitialized());
    EXPECT_EQ(2UL, msg2.cluster_location_size());

    std::vector<vd::ClusterLocationAndHash> tmp;
    for (int i = 0; i < msg2.cluster_location_size(); i++)
    {
        vd::ClusterLocation cl;
        *reinterpret_cast<uint64_t*>(&cl) = msg2.cluster_location(i);
        tmp.emplace_back(vd::ClusterLocationAndHash(cl));
    }

    EXPECT_TRUE(tmp == clp);
}

TEST_F(MessageTest, delete_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::File;

    const auto msg(vfsprotocol::MessageUtils::create_delete_request(vfs::Object(tp, id)));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::DeleteRequest msg2;
    msg2.ParseFromString(s);

    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    check_object_type(msg2, tp);
}

TEST_F(MessageTest, transfer_request)
{
    const vfs::ObjectId id("volume");
    const vfs::ObjectType tp = vfs::ObjectType::Volume;

    const vfs::NodeId node("node");
    const uint64_t timeout = 100;
    const auto msg(vfsprotocol::MessageUtils::create_transfer_request(vfs::Object(tp, id),
                                                                      node,
                                                                      boost::chrono::milliseconds(timeout)));
    ASSERT_TRUE(msg.IsInitialized());

    const std::string s(msg.SerializeAsString());

    vfsprotocol::TransferRequest msg2;
    msg2.ParseFromString(s);

    ASSERT_TRUE(msg2.IsInitialized());

    EXPECT_EQ(id.str(), msg2.object_id());
    check_object_type(msg2, tp);

    EXPECT_EQ(node.str(), msg2.target_node_id());
    EXPECT_EQ(timeout,
              msg2.sync_timeout_ms());
}

}
