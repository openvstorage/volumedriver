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

#include "InitializedParam.h"

#include <iostream>
#include <set>

#include <boost/property_tree/json_parser.hpp>

namespace initialized_params
{

namespace bpt = boost::property_tree;

std::list<ParameterInfo*>&
ParameterInfo::get_parameter_info_list()
{
    static std::list<ParameterInfo*> parameter_infos;
    return parameter_infos;
}

void
ParameterInfo::verify_property_tree(const bpt::ptree& ptree)
{
    bpt::ptree pt(ptree);
    std::set<std::string> components;

    // We need 2 passes as removing nested.keys is not supported:
    // (1) drop all known keys from the components
    // (2) drop all empty components
    // . If anything's left we yell.o
    for (const auto info : get_parameter_info_list())
    {
        components.insert(info->section_name);
        auto it = pt.find(info->section_name);
        if (it != pt.not_found())
        {
            it->second.erase(info->name);
        }
    }

    for (const auto& c : components)
    {
        auto it = pt.find(c);

        if (it != pt.not_found() and it->second.empty())
        {
            pt.erase(c);
        }
    }

    if (not pt.empty())
    {
        LOG_ERROR("property tree contains unknown entries - JSON dump of them follows");
        std::stringstream ss;
        bpt::json_parser::write_json(ss, pt);
        LOG_ERROR(ss.str());

        throw UnknownParameterException("unknown parameters in property tree");
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
