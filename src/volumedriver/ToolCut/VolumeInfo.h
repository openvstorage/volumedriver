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

#ifndef _VOLUME_INFO_H_
#define _VOLUME_INFO_H_

#include <boost/python/dict.hpp>

#include "../VolumeConfig.h"
#include "../FailOverCacheConfigWrapper.h"

namespace toolcut
{

namespace vd = volumedriver;
namespace bp = boost::python;

// Should be renamed VolumeConfigToolCut
class VolumeInfo
{
public:
    VolumeInfo(const std::string& volume_config_path,
               const std::string& failover_config_path);

    VolumeInfo(boost::python::object&,
               const std::string& ns);

    VolumeInfo(const VolumeInfo&) = delete;

    VolumeInfo&
    operator=(const VolumeInfo&) = delete;

    std::string
    volumeID() const
    {
        return volume_config_.id_;
    }

    std::string
    volumeNamespace() const
    {
        return volume_config_.getNS().str();
    }

    std::string
    parentNamespace() const
    {
        if(volume_config_.parent())
        {

            return volume_config_.parent()->nspace.str();
        }
        else
        {
            return "";
        }
    }

    std::string
    parentSnapshot() const
    {
        if(volume_config_.parent())
        {

            return volume_config_.parent()->snapshot;
        }
        else
        {
            return "";
        }
    }

    uint32_t
    lbaSize() const
    {
        return volume_config_.lba_size_;
    }

    uint64_t
    lbaCount() const
    {
        return volume_config_.lba_count();
    }

    uint32_t
    clusterMultiplier() const
    {
        return volume_config_.cluster_mult_;
    }

    uint32_t
    scoMultiplier() const
    {
        return volume_config_.sco_mult_;
    }

    bool
    readCacheEnabled() const
    {
        return volume_config_.readCacheEnabled_;
    }

    vd::FailOverCacheConfigWrapper::CacheType
    failOverCacheType() const
    {
        return foc_config_wrapper_.getCacheType();
    }

    std::string
    failOverCacheHost() const
    {
        return foc_config_wrapper_.getCacheHost();
    }

    uint16_t
    failOverCachePort() const
    {
        return foc_config_wrapper_.getCachePort();
    }

    vd::VolumeConfig::WanBackupVolumeRole
    role() const
    {
        return volume_config_.wan_backup_volume_role_;
    }

    uint64_t
    size() const
    {
        return volume_config_.lba_count() * volume_config_.lba_size_;
    }

    std::string
    str() const;

    std::string
    repr() const;

private:
    vd::VolumeConfig volume_config_;
    vd::FailOverCacheConfigWrapper foc_config_wrapper_;
};

}

#endif // _VOLUME_INFO_H_

// Local Variables: **
// mode: c++ **
// End: **
