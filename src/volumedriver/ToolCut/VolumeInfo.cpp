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

#include <boost/python.hpp>
#include "VolumeInfo.h"

#include <boost/filesystem/fstream.hpp>
#include <backend-python/ConnectionInterface.h>
#include <youtils/FileUtils.h>

namespace toolcut
{
using namespace volumedriver;


VolumeInfo::VolumeInfo(boost::python::object& backend,
                       const std::string& ns)
{
    {
        fs::path p(FileUtils::create_temp_file_in_temp_dir(VolumeConfig::config_backend_name));
        ALWAYS_CLEANUP_FILE(p);
        backend.attr("read")(ns,
                             p.string(),
                             VolumeConfig::config_backend_name,
                             true);
        fs::ifstream ifs(p);
        VolumeConfig::iarchive_type ia(ifs);
        ia & volume_config_;
        ifs.close();
    }

    {
        fs::path p(FileUtils::create_temp_file_in_temp_dir(FailOverCacheConfigWrapper::config_backend_name));
        ALWAYS_CLEANUP_FILE(p);
        backend.attr("read")(ns,
                             p.string(),
                             FailOverCacheConfigWrapper::config_backend_name,
                             true);
        fs::ifstream ifs(p);
        FailOverCacheConfigWrapper::iarchive_type ia(ifs);
        ia & foc_config_wrapper_;
        ifs.close();
    }
}

VolumeInfo::VolumeInfo(const std::string& volume_config,
                       const std::string& failover_config)
{
    if(not volume_config.empty())
    {
        fs::ifstream ifs(volume_config);
        VolumeConfig::iarchive_type ia(ifs);
        ia & volume_config_;
        ifs.close();
    }

    if(not failover_config.empty())
    {
        fs::ifstream ifs(failover_config);
        FailOverCacheConfigWrapper::iarchive_type ia(ifs);
        ia & foc_config_wrapper_;
        ifs.close();
    }
}

std::string
VolumeInfo::str() const
{
    std::stringstream ss;
#define P(x) << #x ": " << x() << std::endl

    ss  P(volumeID)
        P(volumeNamespace)
        P(parentNamespace)
        P(parentSnapshot)
        P(lbaSize)
        P(lbaCount)
        P(clusterMultiplier)
        P(scoMultiplier)
        P(readCacheEnabled)
        P(failOverCacheType)
        P(failOverCacheHost)
        P(failOverCachePort)
        P(role)
        P(size);

#undef P

    return ss.str();
}

std::string
VolumeInfo::repr() const
{
    return std::string("< ") + str() + std::string(" >");
}

}

// Local Variables: **
// mode: c++ **
// End: **
