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

#include "ArakoonIniParser.h"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

namespace arakoon
{

namespace bpt = boost::property_tree;

namespace
{

const std::string cluster_id_key("global.cluster_id");
const std::string cluster_nodes_key("global.cluster");
const std::string port_key_suffix(".client_port");
const std::string ip_key_suffix(".ip");

}

IniParser::IniParser(std::istream& is)
{
    bpt::ptree pt;
    bpt::ini_parser::read_ini(is,
                              pt);

    cluster_id_ = pt.get<ClusterID>(cluster_id_key);

    std::vector<std::string> node_ids;
    const auto node_ids_str(pt.get<std::string>(cluster_nodes_key));

    boost::split(node_ids,
                 node_ids_str,
                 boost::is_any_of(" ,"),
                 boost::token_compress_on);

    node_configs_.reserve(node_ids.size());

    for (const auto& n : node_ids)
    {
        const auto ip(pt.get<std::string>(n + ip_key_suffix));
        const auto port(pt.get<uint16_t>(n + port_key_suffix));

        node_configs_.emplace_back(ArakoonNodeConfig(NodeID(n),
                                                     ip,
                                                     port));
    }
}

}
