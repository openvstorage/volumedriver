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

#include "MDSTestSetup.h"

#include "../metadata-server/Manager.h"

#include <boost/property_tree/ptree.hpp>

#include <youtils/FileUtils.h>
#include <youtils/TestBase.h>

#include <backend/BackendTestSetup.h>

namespace volumedrivertest
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace yt = youtils;
namespace ytt = youtilstest;

class MDSManagerTest
    : public ytt::TestBase
    , public be::BackendTestSetup
{
public:
    MDSManagerTest()
        : home_(yt::FileUtils::temp_path() / "MDSManagerTest")
    {
        fs::remove_all(home_);
        fs::create_directories(home_);

        initialize_connection_manager();

        mds_test_setup_ = std::make_unique<MDSTestSetup>(home_ / "mds");
        mds_manager_ = mds_test_setup_->make_manager(cm_,
                                                     0);
    }

    ~MDSManagerTest()
    {
        mds_manager_.reset();
        mds_test_setup_.reset();

        uninitialize_connection_manager();

        fs::remove_all(home_);
    }

protected:
    const fs::path home_;
    std::unique_ptr<MDSTestSetup> mds_test_setup_;
    std::unique_ptr<mds::Manager> mds_manager_;
};

TEST_F(MDSManagerTest, start_and_stop_servers)
{
    ASSERT_TRUE(mds_manager_->server_configs().empty());

    size_t count = 7;
    mds::ServerConfigs scfgs;
    scfgs.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        mds::ServerConfig scfg(mds_test_setup_->next_server_config());
        mds_manager_->start_one(scfg);
        scfgs.emplace_back(scfg);
    }

    ASSERT_EQ(scfgs,
              mds_manager_->server_configs());

    for (const auto& scfg : scfgs)
    {
        mds_manager_->stop_one(scfg.node_config);
    }

    ASSERT_TRUE(mds_manager_->server_configs().empty());
}

TEST_F(MDSManagerTest, start_twice)
{
    const mds::ServerConfig scfg(mds_test_setup_->next_server_config());

    ASSERT_TRUE(mds_manager_->server_configs().empty());

    mds_manager_->start_one(scfg);

    mds::ServerConfigs scfgs(mds_manager_->server_configs());
    ASSERT_EQ(1U,
              scfgs.size());

    ASSERT_EQ(scfg,
              scfgs[0]);

    ASSERT_THROW(mds_manager_->start_one(scfg),
                 std::exception);

    scfgs = mds_manager_->server_configs();
    ASSERT_EQ(1U,
              scfgs.size());

    ASSERT_EQ(scfg,
              scfgs[0]);
}

TEST_F(MDSManagerTest, conflicting_configs)
{
    const mds::ServerConfig scfg(mds_test_setup_->next_server_config());
    mds_manager_->start_one(scfg);

    // different node config but same directory
    mds::ServerConfig scfg2 = scfg;
    scfg2.node_config = vd::MDSNodeConfig(scfg.node_config.address(),
                                          scfg.node_config.port() + 1);

    ASSERT_THROW(mds_manager_->start_one(scfg2),
                 std::exception);

    // same node config, different directories
    scfg2 = mds_test_setup_->next_server_config();
    scfg2.node_config = scfg.node_config;

    ASSERT_THROW(mds_manager_->start_one(scfg2),
                 std::exception);

    const mds::ServerConfigs scfgs(mds_manager_->server_configs());
    ASSERT_EQ(1U,
              scfgs.size());

    ASSERT_EQ(scfg,
              scfgs[0]);
}

TEST_F(MDSManagerTest, stop_inexistent)
{
    const mds::ServerConfig scfg(mds_test_setup_->next_server_config());

    ASSERT_TRUE(mds_manager_->server_configs().empty());

    ASSERT_THROW(mds_manager_->stop_one(scfg.node_config),
                 std::exception);
}

TEST_F(MDSManagerTest, start_and_stop_with_ptree)
{
    ASSERT_TRUE(mds_manager_->server_configs().empty());

    auto fun([this](const mds::ServerConfigs& scfgs)
             {
                 bpt::ptree pt;
                 mds_test_setup_->make_manager_config(pt,
                                                      scfgs);

                 yt::ConfigurationReport crep;
                 ASSERT_TRUE(mds_manager_->checkConfig(pt,
                                                       crep));

                 ASSERT_TRUE(crep.empty());

                 yt::UpdateReport urep;
                 mds_manager_->update(pt,
                                      urep);

                 const mds::ServerConfigs scfgs1(mds_manager_->server_configs());
                 ASSERT_EQ(scfgs,
                           scfgs1);
             });

    const size_t count = 3;
    mds::ServerConfigs scfgs;
    scfgs.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        scfgs.emplace_back(mds_test_setup_->next_server_config());
    }

    fun(scfgs);

    scfgs = { scfgs[2],
              scfgs[0] };
    fun(scfgs);

    scfgs = {};
    fun(scfgs);

    ASSERT_TRUE(mds_manager_->server_configs().empty());
}

TEST_F(MDSManagerTest, conflicting_ptree_updates)
{
    auto fun([this](const mds::ServerConfigs& scfgs)
             {
                 const mds::ServerConfigs before(mds_manager_->server_configs());

                 bpt::ptree pt;
                 mds_test_setup_->make_manager_config(pt,
                                                      scfgs);

                 yt::ConfigurationReport crep;
                 ASSERT_FALSE(mds_manager_->checkConfig(pt,
                                                        crep));
                 ASSERT_FALSE(crep.empty());

                 yt::UpdateReport urep;
                 ASSERT_THROW(mds_manager_->update(pt,
                                                   urep),
                              std::exception);

                 const mds::ServerConfigs after(mds_manager_->server_configs());

                 ASSERT_EQ(before,
                           after);
             });

    const mds::ServerConfig scfg(mds_test_setup_->next_server_config());
    mds_manager_->start_one(scfg);

    {
        const mds::ServerConfigs scfgs(mds_manager_->server_configs());
        ASSERT_EQ(1U,
                  scfgs.size());
        ASSERT_EQ(scfg,
                  scfgs[0]);
    }

    fun(mds::ServerConfigs{ scfg, scfg });

    mds::ServerConfig scfg2(mds_test_setup_->next_server_config());
    scfg2.node_config = scfg.node_config;

    fun(mds::ServerConfigs{ scfg, scfg2 });

    mds::ServerConfig scfg3(mds_test_setup_->next_server_config());
    scfg3.db_path = scfg.db_path;

    fun(mds::ServerConfigs{ scfg, scfg3 });
}

TEST_F(MDSManagerTest, find)
{
    const mds::ServerConfig scfg(mds_test_setup_->next_server_config());

    mds::DataBaseInterfacePtr db(mds_manager_->find(scfg.node_config));
    ASSERT_TRUE(db == nullptr);

    mds_manager_->start_one(scfg);

    db = mds_manager_->find(scfg.node_config);
    ASSERT_TRUE(db != nullptr);

    ASSERT_TRUE(db->list_namespaces().empty());

    mds_manager_->stop_one(scfg.node_config);

    ASSERT_THROW(db->list_namespaces(),
                 std::exception);

    db = mds_manager_->find(scfg.node_config);
    ASSERT_TRUE(db == nullptr);
}

}
