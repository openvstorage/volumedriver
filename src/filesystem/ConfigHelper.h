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

#include <volumedriver/DtlTransport.h>

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

    volumedriver::DtlTransport
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
    const volumedriver::DtlTransport failover_cache_transport_;
    boost::optional<ClusterNodeConfig> localnode_config_;
};

}

#endif // !VFS_CONFIG_HELPER_H_
