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

#include <boost/property_tree/ptree.hpp>

#include "../ClusterNodeConfig.h"
#include "../ConfigHelper.h"

namespace bpt = boost::property_tree;
namespace vfs = volumedriverfs;

namespace volumedriverfstest
{

class FailOverCacheHelperTest
    : public FileSystemTestBase
{
protected:
    FailOverCacheHelperTest()
        : FileSystemTestBase(FileSystemTestSetupParameters("DtlTestSetup"))
    {}
};

TEST_F(FailOverCacheHelperTest, empty_ptree)
{
    bpt::ptree pt;

    EXPECT_THROW(vfs::ConfigHelper h(pt),
                 std::exception);
}

TEST_F(FailOverCacheHelperTest, missing_cluster_id)
{
    bpt::ptree pt;
    make_registry_config_(pt);

    EXPECT_THROW(vfs::ConfigHelper h(pt),
                 std::exception);
}

TEST_F(FailOverCacheHelperTest, happy_path)
{
    bpt::ptree pt;
    make_config_(pt, topdir_, local_node_id());

    const vfs::ConfigHelper h(pt);
    EXPECT_EQ(failovercache_dir(topdir_), h.failover_cache_path());
    EXPECT_EQ(local_config().failovercache_port,
              vfs::FailoverCachePort(h.failover_cache_port()));
    EXPECT_EQ(failovercache_transport(),
              h.failover_cache_transport());
}

}
