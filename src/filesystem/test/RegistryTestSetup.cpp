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
