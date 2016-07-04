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

#include "../VolumeConfigParameters.h"
#include "../MDSMetaDataBackend.h"

#include <gtest/gtest.h>

namespace volumedrivertest
{

namespace be = backend;
namespace vd = volumedriver;

class VolumeConfigParametersTest
    : public testing::Test
{};

TEST_F(VolumeConfigParametersTest, vanilla)
{
    const vd::VolumeId id("volume");
    const be::Namespace nspace;
    const vd::VolumeSize size(1ULL << 30);

    vd::VanillaVolumeConfigParameters params(id,
                                             nspace,
                                             size,
                                             vd::OwnerTag(1));

    EXPECT_EQ(id, params.get_volume_id());
    EXPECT_EQ(nspace, params.get_nspace());
    EXPECT_EQ(size, params.get_size());
    EXPECT_EQ(vd::OwnerTag(1), params.get_owner_tag());

    EXPECT_EQ(vd::VolumeConfig::default_lba_size(), params.get_lba_size());
    EXPECT_EQ(vd::VolumeConfig::default_cluster_multiplier(),
              params.get_cluster_multiplier());
    EXPECT_EQ(vd::VolumeConfig::default_sco_multiplier(),
              params.get_sco_multiplier());

    {
        vd::MetaDataBackendConfig& cfg(*params.get_metadata_backend_config());
        EXPECT_EQ(vd::MetaDataBackendType::TCBT,
                  cfg.backend_type());
    }

    EXPECT_FALSE(params.get_parent_nspace());
    EXPECT_FALSE(params.get_parent_snapshot());
    EXPECT_FALSE(params.get_volume_role());
    EXPECT_TRUE(params.get_cluster_cache_enabled());
    EXPECT_FALSE(params.get_cluster_cache_behaviour());
    EXPECT_FALSE(params.get_cluster_cache_mode());

    params.cluster_cache_enabled(false);
    EXPECT_FALSE(params.get_cluster_cache_enabled());

    const std::string host("127.0.0.1");
    const uint16_t port(12345);

    {
        const std::vector<vd::MDSNodeConfig> ncfgs{ vd::MDSNodeConfig(host,
                                                                      port) };

        vd::VolumeConfig::MetaDataBackendConfigPtr
            cfg(new vd::MDSMetaDataBackendConfig(ncfgs,
                                                 vd::ApplyRelocationsToSlaves::T));
	params.metadata_backend_config(cfg);
    }

    {
        const vd::MetaDataBackendConfig& cfg(*params.get_metadata_backend_config());
        EXPECT_EQ(vd::MetaDataBackendType::MDS,
                  cfg.backend_type());
        const auto& mcfg(dynamic_cast<const vd::MDSMetaDataBackendConfig&>(cfg));

        EXPECT_EQ(1U, mcfg.node_configs().size());
        EXPECT_EQ(host, mcfg.node_configs()[0].address());
        EXPECT_EQ(port, mcfg.node_configs()[0].port());
    }
}

}
