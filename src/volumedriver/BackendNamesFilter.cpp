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

#include "BackendNamesFilter.h"
#include "FailOverCacheConfigWrapper.h"
#include "SCOAccessData.h"
#include "SnapshotManagement.h"
#include "VolumeConfig.h"
#include "Types.h"

namespace volumedriver
{

bool
BackendNamesFilter::operator()(const std::string& name) const
{
    return boost::regex_match(name, regex_());
}

const boost::regex&
BackendNamesFilter::regex_()
{
    // we don't use uppercase hex digits in sco / tlog names, so no [[:xdigit:]] but rather
    // [0-9a-f] below.
    static const std::string rexstr(std::string(SCOAccessDataPersistor::backend_name)
                                    + std::string("|")
                                    + FailOverCacheConfigWrapper::config_backend_name
                                    + std::string("|")
                                    + VolumeConfig::config_backend_name
                                    + std::string("|")
                                    + VolumeInterface::owner_tag_backend_name()
                                    + std::string("|")
                                    + snapshotFilename()
                                    + std::string("|")
                                    + std::string("tlog_[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}")
                                    + std::string("|")
                                    + std::string("[0-9a-f]{2}_[0-9a-f]{8}_[0-9a-f]{2}")
                                    );

    static const boost::regex rex(rexstr, boost::regex::extended);
    return rex;
}

}

// Local Variables: **
// mode: c++ **
// End: **
