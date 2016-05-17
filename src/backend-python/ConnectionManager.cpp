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

#include "ConnectionManager.h"
#include "ConnectionInterface.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <backend/BackendParameters.h>

namespace pythonbackend
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace bpy = boost::python;
namespace ip = initialized_params;

ConnectionInterface*
ConnectionManager::getConnection()
{
    return new ConnectionInterface(backend_conn_manager_->getConnection());
}

ConnectionManager::ConnectionManager(const bpy::dict& backend_config)
{
    bpt::ptree pt;

    bpy::list keys = backend_config.keys();
    for (auto i = 0; i < bpy::len(keys); ++i)
    {
        const std::string k = bpy::extract<std::string>(keys[i]);
        const std::string v = bpy::extract<std::string>(backend_config[k]);
        pt.put(k, v);
    }


    root.put("version", "1");
    root.push_back(bpt::ptree::value_type(ip::backend_connection_manager_name, pt));

    _init(root);
}

ConnectionManager::ConnectionManager(const std::string& config_file)
{
    bpt::json_parser::read_json(config_file,
                                root);
    _init(root);
}

void
ConnectionManager::_init(const bpt::ptree& ptree)
{
    backend_conn_manager_ = be::BackendConnectionManager::create(ptree,
                                                                 RegisterComponent::F);
}
using namespace std::string_literals;

std::string
ConnectionManager::str() const
{
    std::stringstream ss;
    write_json(ss,
               root);

    return "<ConnectionManager>\n"s + ss.str();
}

std::string
ConnectionManager::repr() const
{
    std::stringstream ss;
    write_json(ss,
               root);

    return "<ConnectionManager>\n"s;

}

}

// Local Variables: **
// mode: c++ **
// End: **
