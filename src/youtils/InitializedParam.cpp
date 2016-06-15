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
