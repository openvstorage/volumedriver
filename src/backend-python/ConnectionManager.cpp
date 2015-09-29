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
