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

#include "FileSystemTestBase.h"

#include <youtils/ConfigurationReport.h>
#include <youtils/UpdateReport.h>

namespace volumedriverfstest
{

using namespace volumedriverfs;

namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace vd = volumedriver;
namespace yt = youtils;

#define LOCKVD() \
    fungi::ScopedLock ag__(api::getManagementMutex())

class ManualDtlConfigModeTest
    : public FileSystemTestBase
{
public:
    ManualDtlConfigModeTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("ManualDtlConfigModeTest")
                             .dtl_config_mode(FailOverCacheConfigMode::Manual))
    {}
};

TEST_F(ManualDtlConfigModeTest, reconfiguration)
{
    const FrontendPath path(make_volume_name("/volume"));
    const ObjectId oid(create_file(path));
    const vd::VolumeId vid(oid.str());

    ObjectRouter& router = fs_->object_router();

    EXPECT_EQ(FailOverCacheConfigMode::Manual,
              router.get_default_foc_config_mode());

    const ClusterNodeConfig ncfg(local_config());
    const ObjectRouter::MaybeFailOverCacheConfig fcfg(router.failoverconfig_as_it_should_be());

    ASSERT_NE(boost::none,
              fcfg);
    EXPECT_EQ(ncfg.failovercache_host,
              fcfg->host);
    EXPECT_EQ(ncfg.failovercache_port,
              fcfg->port);
    EXPECT_EQ(dtl_mode_,
              fcfg->mode);

    {
        const ObjectRegistrationPtr reg(router.object_registry()->find(oid,
                                                                       IgnoreCache::F));
        EXPECT_EQ(FailOverCacheConfigMode::Automatic,
                  reg->foc_config_mode);
    }

    {
        LOCKVD();
        EXPECT_EQ(fcfg,
                  api::getFailOverCacheConfig(vd::VolumeId(vid)));
    }

    auto set_dtl_host([&](const std::string& h)
                      {
                          LOCKVD();

                          bpt::ptree pt;
                          api::persistConfiguration(pt,
                                                    false);
                          ip::PARAMETER_TYPE(fs_dtl_host)(h).persist(pt);
                          api::updateConfiguration(pt);
                      });

    set_dtl_host("");

    EXPECT_EQ(boost::none,
              router.failoverconfig_as_it_should_be());

    {
        LOCKVD();
        EXPECT_EQ(boost::none,
                  api::getFailOverCacheConfig(vid));
    }

    set_dtl_host(fcfg->host);

    EXPECT_EQ(fcfg,
              router.failoverconfig_as_it_should_be());

    {
        LOCKVD();
        EXPECT_EQ(fcfg,
                  api::getFailOverCacheConfig(vd::VolumeId(vid)));
    }
}

}
