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

#include "ClusterNodeConfig.h"
#include "ClusterRegistry.h"
#include "ConfigHelper.h"
#include "FileSystemParameters.h"
#include "LockedArakoon.h"
#include "NodeId.h"
#include "Registry.h"

#include <youtils/InitializedParam.h>

namespace bpt = boost::property_tree;

namespace volumedriverfs
{

ConfigHelper::ConfigHelper(const bpt::ptree& pt)
    : failover_cache_path_(PARAMETER_VALUE_FROM_PROPERTY_TREE(failovercache_path,
                                                              pt))
    , failover_cache_transport_(PARAMETER_VALUE_FROM_PROPERTY_TREE(failovercache_transport,
                                                                   pt))
{
    const NodeId vrouter_id(PARAMETER_VALUE_FROM_PROPERTY_TREE(vrouter_id,
                                                               pt));

    cluster_id_ = ClusterId(PARAMETER_VALUE_FROM_PROPERTY_TREE(vrouter_cluster_id,
                                                               pt));

    std::shared_ptr<LockedArakoon>
        arakoon(std::static_pointer_cast<LockedArakoon>(std::make_shared<Registry>(pt)));
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
