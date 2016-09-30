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

#include "MDSTestSetup.h"
#include "VolManagerTestSetup.h"

#include <youtils/FileUtils.h>
#include <gtest/gtest.h>

#include <backend/BackendTestSetup.h>

#include "../ClusterLocation.h"
#include "../ClusterLocationAndHash.h"
#include "../MDSMetaDataStore.h"
#include "../MetaDataBackendConfig.h"

#include "../metadata-server/Manager.h"

namespace volumedrivertest
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace mds = metadata_server;
namespace vd = volumedriver;
namespace yt = youtils;

class MDSMetaDataStoreTest
    : public testing::Test
    , public be::BackendTestSetup
{
public:
    MDSMetaDataStoreTest()
        : be::BackendTestSetup()
        , root_(yt::FileUtils::temp_path() / "MDSMetaDataStoreTest")
    {
        fs::remove_all(root_);
        fs::create_directories(root_);
        initialize_connection_manager();

        mds_test_setup_ = std::make_unique<MDSTestSetup>(root_ / "mds");
        mds_manager_ = mds_test_setup_->make_manager(cm_,
                                                     1);
    }

    virtual ~MDSMetaDataStoreTest()
    {
        mds_manager_.reset();
        mds_test_setup_.reset();
        uninitialize_connection_manager();

        fs::remove_all(root_);
    }

    const fs::path
    mdstore_home() const
    {
        return root_ / "mds-mdstore";
    }

protected:
    const fs::path root_;
    std::unique_ptr<MDSTestSetup> mds_test_setup_;
    std::unique_ptr<mds::Manager> mds_manager_;
};

TEST_F(MDSMetaDataStoreTest, construction_with_empty_configs)
{
    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    std::vector<vd::MDSNodeConfig> empty;
    EXPECT_THROW(std::make_unique<vd::MDSMetaDataStore>(vd::MDSMetaDataBackendConfig(empty,
                                                                                     vd::ApplyRelocationsToSlaves::T),
                                                        cm_->newBackendInterface(wrns.ns()),
                                                        mdstore_home(),
                                                        vd::OwnerTag(1),
                                                        1024),
                 std::exception);
}

TEST_F(MDSMetaDataStoreTest, construction_with_awol_server)
{
    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    const vd::MDSNodeConfigs
        ncfgs{ mds_test_setup_->next_server_config().node_config };
    const vd::MDSMetaDataBackendConfig cfg(ncfgs,
                                           vd::ApplyRelocationsToSlaves::T);

    EXPECT_THROW(std::make_unique<vd::MDSMetaDataStore>(cfg,
                                                        cm_->newBackendInterface(wrns.ns()),
                                                        mdstore_home(),
                                                        vd::OwnerTag(1),
                                                        1024),
                 std::exception);
}

TEST_F(MDSMetaDataStoreTest, construction_with_awol_master_and_present_slave)
{
    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    const mds::ServerConfigs scfgs(mds_manager_->server_configs());
    ASSERT_EQ(1U,
              scfgs.size());

    const vd::MDSNodeConfigs ncfgs{ mds_test_setup_->next_server_config().node_config,
                                    scfgs[0].node_config };

    const vd::MDSMetaDataBackendConfig mcfg(ncfgs,
                                            vd::ApplyRelocationsToSlaves::T);

    auto mdstore(std::make_unique<vd::MDSMetaDataStore>(mcfg,
                                                        cm_->newBackendInterface(wrns.ns()),
                                                        mdstore_home(),
                                                        vd::OwnerTag(1),
                                                        1024));

    const vd::MDSNodeConfigs ncfgs2(mdstore->get_config().node_configs());

    ASSERT_EQ(ncfgs.size(), ncfgs2.size());
    ASSERT_EQ(2U, ncfgs2.size());
    EXPECT_EQ(ncfgs.front(), ncfgs2.back());
    EXPECT_EQ(ncfgs.back(), ncfgs2.front());
}

TEST_F(MDSMetaDataStoreTest, successful_construction)
{
    be::BackendTestSetup::WithRandomNamespace wrns("",
                                                   cm_);

    const vd::MDSNodeConfigs ncfgs{ mds_manager_->server_configs()[0].node_config };
    const vd::MDSMetaDataBackendConfig cfg(ncfgs,
                                           vd::ApplyRelocationsToSlaves::T);

    auto mdstore(std::make_unique<vd::MDSMetaDataStore>(cfg,
                                                        cm_->newBackendInterface(wrns.ns()),
                                                        mdstore_home(),
                                                        vd::OwnerTag(1),
                                                        1024));

    EXPECT_EQ(ncfgs[0],
              mdstore->current_node());
    EXPECT_TRUE(ncfgs == mdstore->get_config().node_configs());
}

}
