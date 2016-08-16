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

#include "ArakoonConfigFetcher.h"
#include "ArakoonIniParser.h"
#include "ArakoonInterface.h"

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace youtils
{

namespace ara = arakoon;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;

namespace
{

const std::string ini_key("ini");

}

bpt::ptree
ArakoonConfigFetcher::operator()(VerifyConfig verify_config)
{
    LOG_INFO("Fetching config from " << url_);

    auto it = url_.query().find(ini_key);
    if (it == url_.query().end())
    {
        LOG_ERROR("No '" << ini_key << "' key found in URL");
        throw Exception("No arakoon ini specified");
    }

    const boost::optional<std::string> maybe_path(it->second);
    if (not maybe_path)
    {
        LOG_ERROR("Config path is absent from URL");
        throw Exception("No arakoon config path specified");
    }

    fs::ifstream ifs(*maybe_path);
    const ara::IniParser parser(ifs);
    ifs.close();

    if (ara::ClusterID(url_.host()) != parser.cluster_id())
    {
        LOG_ERROR("URL's cluster ID " << url_.host() <<
                  " doesn't match the config's " << parser.cluster_id());
        throw Exception("Cluster ID mismatch");
    }

    ara::Cluster cluster(parser.cluster_id(),
                         parser.node_configs());

    std::stringstream ss(cluster.get<std::string,
                                     std::string>(url_.path()));

    return VolumeDriverComponent::read_config(ss,
                                              verify_config);
}

}
