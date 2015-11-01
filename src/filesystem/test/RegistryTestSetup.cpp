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

#include "RegistryTestSetup.h"

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>

#include <youtils/InitializedParam.h>

#include "../FileSystemParameters.h"

namespace volumedriverfstest
{

namespace ara = arakoon;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;
namespace vfs = volumedriverfs;
namespace ytt = youtilstest;

RegistryTestSetup::RegistryTestSetup(const std::string& name)
    : ytt::TestBase()
    , ara::ArakoonTestSetup(getTempPath(name) / "arakoon")
    , root_(getTempPath(name))
{}

void
RegistryTestSetup::SetUp()
{
    fs::remove_all(root_);

    setUpArakoon();

    bpt::ptree pt;

    const std::string cluster_id(ara::ArakoonTestSetup::clusterID().str());
    ip::PARAMETER_TYPE(vregistry_arakoon_cluster_id)(cluster_id).persist(pt);

    const auto ara_node_list(ara::ArakoonTestSetup::node_configs());
    const ip::PARAMETER_TYPE(vregistry_arakoon_cluster_nodes)::ValueType
        ara_node_vec(ara_node_list.begin(),
                     ara_node_list.end());
    ip::PARAMETER_TYPE(vregistry_arakoon_cluster_nodes)(ara_node_vec).persist(pt);

    registry_ = std::make_shared<vfs::Registry>(pt);
}

void
RegistryTestSetup::TearDown()
{
    tearDownArakoon(true);
    registry_ = nullptr;
    fs::remove_all(root_);
}

}
