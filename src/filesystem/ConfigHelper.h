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

#ifndef VFS_CONFIG_HELPER_H_
#define VFS_CONFIG_HELPER_H_

#include <string>

#include "ClusterId.h"
#include "ClusterNodeConfig.h"

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include <youtils/Assert.h>
#include <youtils/Logging.h>

#include <volumedriver/FailOverCacheTransport.h>

namespace volumedriverfs
{

class ConfigHelper
{
public:
    explicit ConfigHelper(const boost::property_tree::ptree& pt);

    ~ConfigHelper() = default;

    ConfigHelper(const ConfigHelper&) = delete;

    ConfigHelper&
    operator=(const ConfigHelper&) = delete;

    boost::filesystem::path
    failover_cache_path() const
    {
        return failover_cache_path_;
    }

    volumedriver::FailOverCacheTransport
    failover_cache_transport() const
    {
        return failover_cache_transport_;
    }

    uint16_t
    failover_cache_port() const
    {
        VERIFY(localnode_config_);
        return localnode_config_->failovercache_port;
    }

    const ClusterNodeConfig&
    localnode_config() const
    {
        return *localnode_config_;
    }

    ClusterId
    cluster_id() const
    {
        return cluster_id_;
    }

private:
    DECLARE_LOGGER("FailOverCacheHelper");

    ClusterId cluster_id_;
    const boost::filesystem::path failover_cache_path_;
    const volumedriver::FailOverCacheTransport failover_cache_transport_;
    boost::optional<ClusterNodeConfig> localnode_config_;
};

}

#endif // !VFS_CONFIG_HELPER_H_
