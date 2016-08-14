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

#include "../ArakoonIniParser.h"
#include "../ArakoonTestSetup.h"
#include "../FileUtils.h"

#include <gtest/gtest.h>

namespace arakoontest
{

using namespace arakoon;
using namespace std::literals::string_literals;

namespace fs = boost::filesystem;
namespace yt = youtils;

class ArakoonIniParserTest
    : public testing::Test
{
protected:
    ArakoonIniParserTest()
        : root_(yt::FileUtils::temp_path("ArakoonIniParserTest"))
    {}

    const fs::path root_;
};

TEST_F(ArakoonIniParserTest, dogfood)
{
    const size_t node_count = 3;
    ArakoonTestSetup ts(root_,
                        node_count);

    std::stringstream ss;
    ts.write_config(ss);

    const IniParser p(ss);

    EXPECT_EQ(ts.clusterID(),
              p.cluster_id());

    const std::list<ArakoonNodeConfig> nodes_list(ts.node_configs());
    EXPECT_EQ(node_count,
              nodes_list.size());

    std::set<ArakoonNodeConfig> nodes_set;
    for (const auto& n : nodes_list)
    {
        nodes_set.insert(n);
    }

    EXPECT_EQ(node_count,
              nodes_set.size());

    for (const auto& n : p.node_configs())
    {
        nodes_set.erase(n);
    }

    EXPECT_TRUE(nodes_set.empty());
}

TEST_F(ArakoonIniParserTest, step_by_step)
{
    std::string str;

    auto add_and_check([&](const std::string& s)
                       {
                           str += s + "\n"s;
                           std::stringstream ss(str);
                           EXPECT_THROW(IniParser p(ss),
                                        std::exception) << str;
             });

    add_and_check(""s);

    const std::string cluster_pfx("cluster = ");
    const std::string cluster_id_pfx("cluster_id = ");
    const std::string ip_pfx("ip = ");
    const std::string port_pfx("client_port = ");

    auto section([](const std::string& section) -> std::string
                 {
                     return "["s + section + "]"s;
                 });

    add_and_check(section("global"));

    const ClusterID cluster_id("cluster-id");
    add_and_check(cluster_id_pfx + cluster_id.str());

    const std::vector<ArakoonNodeConfig> node_configs = { ArakoonNodeConfig(NodeID("alpha"),
                                                                            "127.0.0.1"s,
                                                                            1234),
                                                          ArakoonNodeConfig(NodeID("beta"),
                                                                            "127.0.0.1"s,
                                                                            5678),
    };

    add_and_check(cluster_pfx +
                  node_configs[0].node_id_.str() +
                  ", "s +
                  node_configs[1].node_id_.str());

    add_and_check(section(node_configs[0].node_id_.str()));
    add_and_check(ip_pfx + node_configs[0].hostname_);
    add_and_check(port_pfx + boost::lexical_cast<std::string>(node_configs[0].port_));

    add_and_check(section(node_configs[1].node_id_.str()));
    add_and_check(ip_pfx + node_configs[1].hostname_);
    str += port_pfx + boost::lexical_cast<std::string>(node_configs[1].port_);

    std::stringstream ss(str);
    const IniParser p(ss);

    EXPECT_EQ(cluster_id,
              p.cluster_id());
    EXPECT_EQ(node_configs,
              p.node_configs());
}

}
