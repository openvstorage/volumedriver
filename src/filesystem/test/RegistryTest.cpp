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

#include "RegistryTestSetup.h"

namespace volumedriverfstest
{

namespace ara = arakoon;
namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace vfs = volumedriverfs;
namespace yt = youtils;

class RegistryTest
    : public RegistryTestSetup
{
protected:
    RegistryTest()
        : RegistryTestSetup("RegistryTest")
    {}

    virtual ~RegistryTest() = default;

    void
    put(const std::string& key,
        const std::string val)
    {
        registry_->run_sequence("set a record",
                                [&](ara::sequence& seq)
                                {
                                    seq.add_set(key,
                                                val);
                                },
                                vfs::RetryOnArakoonAssert::F);
    }

    const std::string
    get(const std::string& key)
    {
        return registry_->get<std::string,
                              std::string,
                              ara::DataBufferTraits<std::string>,
                              ara::DataBufferTraits<std::string>>(key);
    }
};

TEST_F(RegistryTest, check_config_ok)
{
    bpt::ptree pt;
    registry_->persist(pt,
                       ReportDefault::T);

    yt::ConfigurationReport crep;

    ASSERT_TRUE(registry_->checkConfig(pt,
                                       crep));

    ASSERT_TRUE(crep.empty());
}

TEST_F(RegistryTest, check_config_nok)
{
    bpt::ptree pt;

    registry_->persist(pt,
                       ReportDefault::T);

    std::vector<ara::ArakoonNodeConfig> cfgs;
    const ip::PARAMETER_TYPE(vregistry_arakoon_cluster_nodes) nodes(cfgs);

    nodes.persist(pt);

    yt::ConfigurationReport crep;

    ASSERT_FALSE(registry_->checkConfig(pt,
                                        crep));

    ASSERT_EQ(1U, crep.size());

    const auto& prob = crep.front();

    ASSERT_EQ(nodes.name(),
              prob.param_name());
    ASSERT_EQ(nodes.section_name(),
              prob.component_name());
}

TEST_F(RegistryTest, reconfigure_ok)
{
    const std::string key("key");
    const std::string val("val");

    put(key,
        val);

    bpt::ptree pt;
    registry_->persist(pt,
                       ReportDefault::T);

    const ip::PARAMETER_TYPE(vregistry_arakoon_cluster_nodes) old_nodes(pt);

    std::vector<ara::ArakoonNodeConfig> new_configs;
    new_configs.reserve(old_nodes.value().size());

    for (const auto& cfg : old_nodes.value())
    {
        ASSERT_EQ("127.0.0.1",
                  cfg.hostname_);

        new_configs.emplace_back(ara::ArakoonNodeConfig(cfg.node_id_,
                                                        "localhost",
                                                        cfg.port_));
    }

    ASSERT_LT(0U,
              new_configs.size());

    const ip::PARAMETER_TYPE(vregistry_arakoon_timeout_ms) timeout(2000);

    timeout.persist(pt);

    {
        const decltype(old_nodes) new_nodes(new_configs);

        new_nodes.persist(pt);

        yt::UpdateReport urep;
        registry_->update(pt,
                          urep);

        EXPECT_EQ(2U,
                  urep.update_size());
    }

    registry_->persist(pt,
                       ReportDefault::T);

    const decltype(old_nodes) new_nodes(pt);
    EXPECT_EQ(new_configs,
              new_nodes.value());

    EXPECT_EQ(val,
              get(key));
}

TEST_F(RegistryTest, reconfigure_nok)
{
    const std::string key("key");
    const std::string val("val");

    put(key,
        val);

    bpt::ptree pt;
    registry_->persist(pt,
                       ReportDefault::T);

    const ip::PARAMETER_TYPE(vregistry_arakoon_cluster_nodes) old_nodes(pt);
    const ip::PARAMETER_TYPE(vregistry_arakoon_timeout_ms) timeout(2000);

    timeout.persist(pt);

    std::vector<ara::ArakoonNodeConfig> new_configs;

    {
        const decltype(old_nodes) new_nodes(new_configs);

        new_nodes.persist(pt);

        yt::UpdateReport urep;
        registry_->update(pt,
                          urep);

        EXPECT_EQ(1U,
                  urep.update_size());
    }

    registry_->persist(pt,
                       ReportDefault::T);

    const decltype(old_nodes) new_nodes(pt);
    EXPECT_EQ(old_nodes.value(),
              new_nodes.value());

    EXPECT_EQ(val,
              get(key));
}

}
