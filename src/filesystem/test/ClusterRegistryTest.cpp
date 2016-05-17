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

#include <boost/lexical_cast.hpp>

#include <youtils/InitializedParam.h>
#include <youtils/TestBase.h>
#include <youtils/UUID.h>

#include "../ClusterRegistry.h"
#include "../Registry.h"

namespace volumedriverfstest
{

namespace vfs = volumedriverfs;
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
        cluster_registry_ = std::make_shared<vfs::ClusterRegistry>(cluster_id_,
                                                                   registry_);
    }

    vfs::ClusterNodeConfigs
    make_cluster_node_configs(unsigned count)
    {
        vfs::ClusterNodeConfigs configs;
        configs.reserve(count);

        for (unsigned i = 0; i < count; ++i)
        {
            const std::string n("node" + boost::lexical_cast<std::string>(i));
            configs.push_back(vfs::ClusterNodeConfig(vfs::NodeId(n),
                                                     n,
                                                     vfs::MessagePort(i + 1),
                                                     vfs::XmlRpcPort(i + 1),
                                                     vfs::FailoverCachePort(i + 1)));
        }

        return configs;
    }

    const vfs::ClusterId cluster_id_;
    std::shared_ptr<vfs::ClusterRegistry> cluster_registry_;
};

TEST_F(ClusterRegistryTest, empty_registry)
{
    EXPECT_THROW(cluster_registry_->get_node_configs(),
                 vfs::ClusterNotRegisteredException);

    EXPECT_THROW(cluster_registry_->get_node_status_map(),
                 vfs::ClusterNotRegisteredException);

    EXPECT_THROW(cluster_registry_->get_node_state(vfs::NodeId("some-node")),
                 vfs::ClusterNotRegisteredException);

    EXPECT_THROW(cluster_registry_->set_node_state(vfs::NodeId("some-node"),
                                                   vfs::ClusterNodeStatus::State::Offline),
                 vfs::ClusterNotRegisteredException);

    EXPECT_NO_THROW(cluster_registry_->erase_node_configs());
}

TEST_F(ClusterRegistryTest, empty_configs)
{
    vfs::ClusterNodeConfigs configs;
    EXPECT_THROW(cluster_registry_->set_node_configs(configs),
                 vfs::InvalidConfigurationException);
}

TEST_F(ClusterRegistryTest, config_with_duplicates)
{
    vfs::ClusterNodeConfigs configs = make_cluster_node_configs(1);
    configs.push_back(configs[0]);

    EXPECT_THROW(cluster_registry_->set_node_configs(configs),
                 vfs::InvalidConfigurationException);
}

TEST_F(ClusterRegistryTest, happy_path)
{
    const unsigned count = 3;
    const vfs::ClusterNodeConfigs configs(make_cluster_node_configs(count));

    cluster_registry_->set_node_configs(configs);

    const vfs::ClusterNodeConfigs
        retrieved_configs(cluster_registry_->get_node_configs());
    EXPECT_TRUE(configs == retrieved_configs);

    vfs::ClusterRegistry::NodeStatusMap
        status_map(cluster_registry_->get_node_status_map());

    EXPECT_EQ(configs.size(), status_map.size());

    for (const auto& cfg : configs)
    {
        auto it = status_map.find(cfg.vrouter_id);
        ASSERT_TRUE(it != status_map.end());
        EXPECT_TRUE(cfg == it->second.config);
        EXPECT_EQ(vfs::ClusterNodeStatus::State::Online, it->second.state);

        status_map.erase(it);
    }

    EXPECT_TRUE(status_map.empty());
}

TEST_F(ClusterRegistryTest, update_configs)
{
    const vfs::ClusterNodeConfigs configs(make_cluster_node_configs(3));

    cluster_registry_->set_node_configs(configs);
    EXPECT_TRUE(configs == cluster_registry_->get_node_configs());

    const vfs::ClusterNodeConfigs updated_configs(++configs.begin(), configs.end());
    ASSERT_EQ(configs.size() - 1, updated_configs.size());
    ASSERT_FALSE(updated_configs.empty());

    cluster_registry_->set_node_configs(updated_configs);
    EXPECT_TRUE(updated_configs == cluster_registry_->get_node_configs());

    cluster_registry_->set_node_configs(configs);
    EXPECT_TRUE(configs == cluster_registry_->get_node_configs());
}

TEST_F(ClusterRegistryTest, erasure)
{
    const vfs::ClusterNodeConfigs configs(make_cluster_node_configs(1));
    cluster_registry_->set_node_configs(configs);

    const vfs::ClusterNodeConfigs
        retrieved_configs(cluster_registry_->get_node_configs());
    EXPECT_TRUE(configs == retrieved_configs);

    cluster_registry_->erase_node_configs();

    EXPECT_THROW(cluster_registry_->get_node_configs(),
                 vfs::ClusterNotRegisteredException);
}

TEST_F(ClusterRegistryTest, node_states)
{
    const vfs::ClusterNodeConfigs configs(make_cluster_node_configs(1));
    cluster_registry_->set_node_configs(configs);

    EXPECT_EQ(vfs::ClusterNodeStatus::State::Online,
              cluster_registry_->get_node_state(configs[0].vrouter_id));

    cluster_registry_->set_node_state(configs[0].vrouter_id,
                                      vfs::ClusterNodeStatus::State::Offline);

    EXPECT_EQ(vfs::ClusterNodeStatus::State::Offline,
              cluster_registry_->get_node_state(configs[0].vrouter_id));

    const vfs::ClusterRegistry::NodeStatusMap
        status_map(cluster_registry_->get_node_status_map());
    ASSERT_EQ(1U, status_map.size());
    EXPECT_EQ(vfs::ClusterNodeStatus::State::Offline,
              status_map.begin()->second.state);

    EXPECT_THROW(cluster_registry_->get_node_state(vfs::NodeId("InexistentNode")),
                 vfs::ClusterNodeNotRegisteredException);

    EXPECT_THROW(cluster_registry_->set_node_state(vfs::NodeId("InexistentNode"),
                                                   vfs::ClusterNodeStatus::State::Offline),
                 vfs::ClusterNodeNotRegisteredException);
}

TEST_F(ClusterRegistryTest, unique_key)
{
    const vfs::ClusterId cluster_id(yt::UUID().str());
    vfs::ClusterRegistry cregistry(cluster_id,
                                   registry_);

    EXPECT_NE(cregistry.make_key(), cluster_registry_->make_key());
}

}
