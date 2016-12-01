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

#include "RegistryTestSetup.h"

#include "../ClusterRegistry.h"
#include "../Registry.h"

#include <boost/lexical_cast.hpp>
#include <boost/optional/optional_io.hpp>

#include <gtest/gtest.h>

#include <youtils/InitializedParam.h>
#include <youtils/UUID.h>

namespace volumedriverfstest
{

using namespace volumedriverfs;
namespace yt = youtils;

// TODO: factor out the code that is shared with {Cached,}VolumeRegistryTest
class ClusterRegistryTest
    : public RegistryTestSetup
{
protected:
    ClusterRegistryTest()
        : RegistryTestSetup("ClusterRegistryTest")
        , cluster_id_("ClusterRegistryTestCluster")
    {}

    virtual void
    SetUp()
    {
        RegistryTestSetup::SetUp();
        cluster_registry_ = std::make_shared<ClusterRegistry>(cluster_id_,
                                                              registry_);
    }

    ClusterNodeConfigs
    make_cluster_node_configs(unsigned count)
    {
        ClusterNodeConfigs configs;
        configs.reserve(count);

        for (unsigned i = 0; i < count; ++i)
        {
            const std::string n("node" + boost::lexical_cast<std::string>(i));
            configs.push_back(ClusterNodeConfig(NodeId(n),
                                                n,
                                                MessagePort(i + 1),
                                                XmlRpcPort(i + 1),
                                                FailoverCachePort(i + 1),
                                                boost::none));
        }

        return configs;
    }

    const ClusterId cluster_id_;
    std::shared_ptr<ClusterRegistry> cluster_registry_;
};

TEST_F(ClusterRegistryTest, empty_registry)
{
    EXPECT_THROW(cluster_registry_->get_node_configs(),
                 ClusterNotRegisteredException);

    EXPECT_THROW(cluster_registry_->get_node_status_map(),
                 ClusterNotRegisteredException);

    EXPECT_THROW(cluster_registry_->get_node_status(NodeId("some-node")),
                 ClusterNotRegisteredException);

    EXPECT_THROW(cluster_registry_->set_node_state(NodeId("some-node"),
                                                   ClusterNodeStatus::State::Offline),
                 ClusterNotRegisteredException);

    EXPECT_NO_THROW(cluster_registry_->erase_node_configs());
}

TEST_F(ClusterRegistryTest, empty_configs)
{
    ClusterNodeConfigs configs;
    EXPECT_THROW(cluster_registry_->set_node_configs(configs),
                 InvalidConfigurationException);
}

TEST_F(ClusterRegistryTest, config_with_duplicates)
{
    ClusterNodeConfigs configs = make_cluster_node_configs(1);
    configs.push_back(configs[0]);

    EXPECT_THROW(cluster_registry_->set_node_configs(configs),
                 InvalidConfigurationException);
}

TEST_F(ClusterRegistryTest, happy_path)
{
    const unsigned count = 3;
    const ClusterNodeConfigs configs(make_cluster_node_configs(count));

    cluster_registry_->set_node_configs(configs);

    const ClusterNodeConfigs
        retrieved_configs(cluster_registry_->get_node_configs());
    EXPECT_TRUE(configs == retrieved_configs);

    ClusterRegistry::NodeStatusMap
        status_map(cluster_registry_->get_node_status_map());

    EXPECT_EQ(configs.size(), status_map.size());

    for (const auto& cfg : configs)
    {
        auto it = status_map.find(cfg.vrouter_id);
        ASSERT_TRUE(it != status_map.end());
        EXPECT_TRUE(cfg == it->second.config);
        EXPECT_EQ(ClusterNodeStatus::State::Online, it->second.state);

        status_map.erase(it);
    }

    EXPECT_TRUE(status_map.empty());
}

TEST_F(ClusterRegistryTest, update_configs)
{
    const ClusterNodeConfigs configs(make_cluster_node_configs(3));

    cluster_registry_->set_node_configs(configs);
    EXPECT_TRUE(configs == cluster_registry_->get_node_configs());

    const ClusterNodeConfigs updated_configs(++configs.begin(), configs.end());
    ASSERT_EQ(configs.size() - 1, updated_configs.size());
    ASSERT_FALSE(updated_configs.empty());

    cluster_registry_->set_node_configs(updated_configs);
    EXPECT_TRUE(updated_configs == cluster_registry_->get_node_configs());

    cluster_registry_->set_node_configs(configs);
    EXPECT_TRUE(configs == cluster_registry_->get_node_configs());
}

TEST_F(ClusterRegistryTest, erasure)
{
    const ClusterNodeConfigs configs(make_cluster_node_configs(1));
    cluster_registry_->set_node_configs(configs);

    const ClusterNodeConfigs
        retrieved_configs(cluster_registry_->get_node_configs());
    EXPECT_TRUE(configs == retrieved_configs);

    cluster_registry_->erase_node_configs();

    EXPECT_THROW(cluster_registry_->get_node_configs(),
                 ClusterNotRegisteredException);
}

TEST_F(ClusterRegistryTest, node_states)
{
    const ClusterNodeConfigs configs(make_cluster_node_configs(1));
    cluster_registry_->set_node_configs(configs);

    EXPECT_EQ(ClusterNodeStatus::State::Online,
              cluster_registry_->get_node_status(configs[0].vrouter_id).state);

    cluster_registry_->set_node_state(configs[0].vrouter_id,
                                      ClusterNodeStatus::State::Offline);

    EXPECT_EQ(ClusterNodeStatus::State::Offline,
              cluster_registry_->get_node_status(configs[0].vrouter_id).state);

    const ClusterRegistry::NodeStatusMap
        status_map(cluster_registry_->get_node_status_map());
    ASSERT_EQ(1U, status_map.size());
    EXPECT_EQ(ClusterNodeStatus::State::Offline,
              status_map.begin()->second.state);

    EXPECT_THROW(cluster_registry_->get_node_status(NodeId("InexistentNode")),
                 ClusterNodeNotRegisteredException);

    EXPECT_THROW(cluster_registry_->set_node_state(NodeId("InexistentNode"),
                                                   ClusterNodeStatus::State::Offline),
                 ClusterNodeNotRegisteredException);
}

TEST_F(ClusterRegistryTest, unique_key)
{
    const ClusterId cluster_id(yt::UUID().str());
    ClusterRegistry cregistry(cluster_id,
                              registry_);

    EXPECT_NE(cregistry.make_key(), cluster_registry_->make_key());
}

TEST_F(ClusterRegistryTest, neighbours_default)
{
    const ClusterNodeConfigs configs(make_cluster_node_configs(7));
    cluster_registry_->set_node_configs(configs);

    for (const auto& cfg : configs)
    {
        EXPECT_FALSE(cfg.node_distance_map);

        const ClusterRegistry::NeighbourMap
            nmap(cluster_registry_->get_neighbour_map(cfg.vrouter_id));
        EXPECT_EQ(configs.size(),
                  nmap.size());
        for (const auto& p : nmap)
        {
            EXPECT_EQ(0, p.first);
        }
    }
}

TEST_F(ClusterRegistryTest, invalid_neighbour_config)
{
    const ClusterNodeConfigs configs(make_cluster_node_configs(2));
    cluster_registry_->set_node_configs(configs);

    ClusterNodeConfigs invalid_configs = { configs[0] };
    invalid_configs[0].node_distance_map = {{ configs[1].vrouter_id, 10 }};

    EXPECT_THROW(cluster_registry_->set_node_configs(invalid_configs),
                 InvalidConfigurationException);

    EXPECT_EQ(configs,
              cluster_registry_->get_node_configs());
}

TEST_F(ClusterRegistryTest, explicit_neighbours)
{
    ClusterNodeConfigs configs(make_cluster_node_configs(3));

    for (auto& cfg : configs)
    {
        bool found = false;
        ClusterNodeConfig::NodeDistanceMap map;

        for (size_t i = 0; i < configs.size(); ++i)
        {
            if (found)
            {
                map[configs[i].vrouter_id] = i;
            }
            else if (configs[i].vrouter_id == cfg.vrouter_id)
            {
                found = true;
            }
        }

        cfg.node_distance_map = std::move(map);
    }

    cluster_registry_->set_node_configs(configs);

    for (size_t i = 0; i < configs.size(); ++i)
    {
        const ClusterRegistry::NeighbourMap
            nmap(cluster_registry_->get_neighbour_map(configs[i].vrouter_id));
        EXPECT_EQ(configs.size() - i - 1,
                  nmap.size());

        size_t j = i + 1;

        for (const auto& p : nmap)
        {
            EXPECT_EQ(j,
                      p.first);
            ASSERT_GT(configs.size(),
                      j);
            EXPECT_EQ(configs[j],
                      p.second);
            ++j;
        }
    }
}

}
