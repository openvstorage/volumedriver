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

#ifndef VFS_CONFIG_FETCHER_H_
#define VFS_CONFIG_FETCHER_H_

#include <youtils/EtcdUrl.h>
#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/EtcdReply.h>
#include <youtils/VolumeDriverComponent.h>

#include <boost/property_tree/ptree_fwd.hpp>

namespace volumedriverfs
{

class ConfigFetcher
{
public:
    MAKE_EXCEPTION(Exception, fungi::IOException);

    explicit ConfigFetcher(const std::string& config)
        : config_(config)
    {}

    ~ConfigFetcher() = default;

    boost::property_tree::ptree
    operator()(VerifyConfig);

    std::string
    parse_config(const youtils::EtcdReply::Records& recs);

private:
    DECLARE_LOGGER("VFSConfigFetcher");

    std::string config_;
};

}

#endif // !VFS_CONFIG_FETCHER_H_
