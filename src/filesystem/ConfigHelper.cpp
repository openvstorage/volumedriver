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

#include "ClusterNodeConfig.h"
#include "ClusterRegistry.h"
#include "ConfigHelper.h"
#include "FileSystemParameters.h"
#include "NodeId.h"
#include "Registry.h"

#include <youtils/InitializedParam.h>
#include <youtils/LockedArakoon.h>


namespace volumedriverfs
{

namespace bpt = boost::property_tree;
namespace yt = youtils;

ConfigHelper::ConfigHelper(const bpt::ptree& pt)
    : failover_cache_path_(PARAMETER_VALUE_FROM_PROPERTY_TREE(dtl_path,
                                                              pt))
    , failover_cache_transport_(PARAMETER_VALUE_FROM_PROPERTY_TREE(dtl_transport,
                                                                   pt))
{
    const NodeId vrouter_id(PARAMETER_VALUE_FROM_PROPERTY_TREE(vrouter_id,
                                                               pt));

    cluster_id_ = ClusterId(PARAMETER_VALUE_FROM_PROPERTY_TREE(vrouter_cluster_id,
                                                               pt));

    std::shared_ptr<yt::LockedArakoon>
        arakoon(std::static_pointer_cast<yt::LockedArakoon>(std::make_shared<Registry>(pt)));
    ClusterRegistry cluster_registry(cluster_id_,
                                     arakoon);

    ClusterNodeConfigs configs_(cluster_registry.get_node_configs());
    for(const auto& cfg : configs_)
    {
        if(cfg.vrouter_id == vrouter_id)
        {
            localnode_config_ = cfg;
        }
    }

    if(not localnode_config_)
    {
        LOG_FATAL("Could not find the router id " << vrouter_id <<
                  " specified in the config in the ClusterRegistry");
        throw fungi::IOException("Could not find router id in ClusterRegistry",
                                 vrouter_id.str().c_str());
    }
}


}
