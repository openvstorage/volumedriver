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
        : FileSystemTestBase(FileSystemTestSetupParameters("FailOverCacheTestSetup"))
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
