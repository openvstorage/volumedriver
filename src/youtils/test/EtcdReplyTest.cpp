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

#include "../EtcdReply.h"
#include "../TestBase.h"

#include <boost/lexical_cast.hpp>

namespace youtilstest
{

using namespace youtils;
using namespace std::literals::string_literals;

class EtcdReplyTest
    : public TestBase
{};

// json snippets taken from https://coreos.com/etcd/docs/0.4.7/etcd-api/
TEST_F(EtcdReplyTest, create_reply)
{
    const std::string json("{"
                           "\"action\": \"set\","
                           "\"node\": {"
                           "\"createdIndex\": 2,"
                           "\"key\": \"/message\","
                           "\"modifiedIndex\": 2,"
                           "\"value\": \"Hello world\""
                           "}"
                           "}");

    const EtcdReply reply(json);

    EXPECT_TRUE(boost::none == reply.error());
    EXPECT_TRUE(boost::none == reply.prev_node());

    const EtcdReply::Node node("/message"s,
                               "Hello world"s,
                               false,
                               2,
                               2);

    const boost::optional<EtcdReply::Node> n(reply.node());
    ASSERT_TRUE(boost::none != n);
    EXPECT_TRUE(node == *n);

    const EtcdReply::Records recs(reply.records());

    EXPECT_EQ(1, recs.size());
    auto it = recs.find(node.key);
    ASSERT_TRUE(it != recs.end());
    EXPECT_TRUE(it->second == node);
}

TEST_F(EtcdReplyTest, update_reply)
{
    const std::string json("{"
                           "\"action\": \"set\","
                           "\"node\": {"
                           "\"createdIndex\": 3,"
                           "\"key\": \"/message\","
                           "\"modifiedIndex\": 3,"
                           "\"value\": \"Hello etcd\""
                           "},"
                           "\"prevNode\": {"
                           "\"createdIndex\": 2,"
                           "\"key\": \"/message\","
                           "\"value\": \"Hello world\","
                           "\"modifiedIndex\": 2"
                           "}"
                           "}");

    const EtcdReply::Node node("/message"s,
                               "Hello etcd"s,
                               false,
                               3,
                               3);

    const EtcdReply::Node prev_node("/message"s,
                                    "Hello world"s,
                                    false,
                                    2,
                                    2);

    const EtcdReply reply(json);
    EXPECT_TRUE(reply.error() == boost::none);

    const boost::optional<EtcdReply::Node> p(reply.prev_node());
    ASSERT_TRUE(p != boost::none);
    EXPECT_TRUE(*p == prev_node);

    const boost::optional<EtcdReply::Node> n(reply.node());
    ASSERT_TRUE(n != boost::none);
    EXPECT_TRUE(*n == node);

    const EtcdReply::Records recs(reply.records());

    EXPECT_EQ(1, recs.size());
    auto it = recs.find(node.key);
    ASSERT_TRUE(it != recs.end());
    EXPECT_TRUE(it->second == node);
}

TEST_F(EtcdReplyTest, key_not_found_error)
{
    const std::string json("{"
                           "\"cause\": \"/foo\","
                           "\"errorCode\": 100,"
                           "\"index\": 6,"
                           "\"message\": \"Key not found\""
                           "}");

    const EtcdReply reply(json);
    const boost::optional<EtcdReply::Error> error(reply.error());

    ASSERT_TRUE(error != boost::none);
    EXPECT_EQ(100ULL,
              error->error_code);
    EXPECT_EQ("/foo"s,
              error->cause);
    ASSERT_TRUE(error->index != boost::none);
    EXPECT_EQ(6ULL,
              *error->index);
    EXPECT_EQ("Key not found"s,
              error->message);

    EXPECT_TRUE(reply.node() == boost::none);
    EXPECT_TRUE(reply.prev_node() == boost::none);
    EXPECT_TRUE(reply.records().empty());
}

TEST_F(EtcdReplyTest, directory_listing)
{
    const std::string json("{"
                           "\"action\": \"get\","
                           "\"node\": {"
                           "\"key\": \"/\","
                           "\"dir\": true,"
                           "\"nodes\": ["
                           "{"
                           "\"key\": \"/foo_dir\","
                           "\"dir\": true,"
                           "\"nodes\": ["
                           "{"
                           "\"key\": \"/foo_dir/foo\","
                           "\"value\": \"bar\","
                           "\"modifiedIndex\": 2,"
                           "\"createdIndex\": 2"
                           "}"
                           "],"
                           "\"modifiedIndex\": 2,"
                           "\"createdIndex\": 2"
                           "},"
                           "{"
                           "\"key\": \"/foo\","
                           "\"value\": \"two\","
                           "\"modifiedIndex\": 1,"
                           "\"createdIndex\": 1"
                           "}"
                           "]"
                           "}"
                           "}");

    const EtcdReply reply(json);

    EXPECT_TRUE(reply.error() == boost::none);
    EXPECT_TRUE(reply.prev_node() == boost::none);

    const EtcdReply::Node node("/",
                               boost::none,
                               true);

    const boost::optional<EtcdReply::Node> n(reply.node());
    ASSERT_TRUE(n != boost::none);
    EXPECT_TRUE(node == n);

    const EtcdReply::Records recs(reply.records());

    EXPECT_EQ(4,
              recs.size());

    auto check([&](const std::string& k)
               {
                   const auto it = recs.find(k);
                   EXPECT_TRUE(it != recs.end());
               });

    check("/"s);
    check("/foo_dir"s);
    check("/foo_dir/foo"s);
    check("/foo"s);

    EXPECT_TRUE(recs.find("/bar"s) == recs.end());
}

}
