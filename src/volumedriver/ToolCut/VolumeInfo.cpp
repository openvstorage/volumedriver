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
        fs::path p(FileUtils::create_temp_file_in_temp_dir(DtlConfigWrapper::config_backend_name));
        ALWAYS_CLEANUP_FILE(p);
        backend.attr("read")(ns,
                             p.string(),
                             DtlConfigWrapper::config_backend_name,
                             true);
        fs::ifstream ifs(p);
        DtlConfigWrapper::iarchive_type ia(ifs);
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
        DtlConfigWrapper::iarchive_type ia(ifs);
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
