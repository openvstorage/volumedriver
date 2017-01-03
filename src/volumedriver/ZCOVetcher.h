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

#ifndef ZCO_VETCHER_H
#define ZCO_VETCHER_H

#include "Types.h"

#include <memory>

#include <boost/filesystem/path.hpp>

#include <youtils/IncrementalChecksum.h>
#include <youtils/Logger.h>

#include <backend/Namespace.h>

namespace volumedriver
{

class SCOCache;
class FailOverCacheConfigWrapper;
class VolumeConfig;

class ZCOVetcher
{
public:
    explicit ZCOVetcher(const VolumeConfig&);

    ~ZCOVetcher();

    CachedSCOPtr
    getSCO(SCO sco);

    bool
    checkSCO(ClusterLocation,
             CheckSum::value_type,
             bool retry);

private:
    DECLARE_LOGGER("ZCOVetcher");

    const Namespace ns_;
    SCOCache& sco_cache_;
    const uint64_t scosize_;
    const LBASize lba_size_;
    const ClusterMultiplier cluster_mult_;
    std::pair<boost::filesystem::path, std::unique_ptr<youtils::IncrementalChecksum> > incremental_checksum_;
    std::unique_ptr<FailOverCacheConfigWrapper> foc_config_wrapper_;
};

}

#endif // ZCO_VETCHER_H

// Local Variables: **
// mode: c++ **
// End: **
