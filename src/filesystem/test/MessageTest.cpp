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

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <youtils/TestBase.h>

#include <volumedriver/Types.h>

#include "../MessageUtils.h"

namespace volumedriverfstest
{

namespace vd = volumedriver;
namespace vfs = volumedriverfs;

class MessageTest
    : public youtilstest::TestBase
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
