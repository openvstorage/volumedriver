// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
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
