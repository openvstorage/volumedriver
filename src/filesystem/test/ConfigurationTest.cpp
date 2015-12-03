// Copyright 2015 iNuron NV
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

#include <iostream>

#include <youtils/BuildInfoString.h>
#include <youtils/InitializedParam.h>
#include <youtils/TestBase.h>
#include <youtils/BuildInfo.h>
#include <boost/python/dict.hpp>
#include <boost/python/list.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/str.hpp>
#include <boost/python/import.hpp>
#include <map>
namespace volumedriverfstest
{

class ConfigurationTest
    : public youtilstest::TestBase
{};

TEST_F(ConfigurationTest, print_documentation)
{
    std::cout << "This documentation was created for: " << std::endl
              << youtils::buildInfoString()
              << "Some parameters might not be documented - these should be regarded as internal and should *not* be changed"
              << std::endl;

    for (const auto& info : initialized_params::ParameterInfo::get_parameter_info_list())
    {
        if (T(info->document_))
        {
            std::cout << "* " << info->name << std::endl
                      << "** " <<"section: " <<  info->section_name << std::endl
                      << "** " << info->doc_string << std::endl;
            if (info->has_default)
            {
                std::cout << "** " << "default: " << info->def_ << std::endl;
            }
            else
            {
                std::cout << "** " << "required parameter " << std::endl;
            }

            if (info->dimensioned_value)
            {
                std::cout << "** Dimensioned Literal (\"234MiB\")" << std::endl;
            }
            else
            {
                std::cout << "** Normal Literal " << std::endl;
            }

            if (info->can_be_reset)
            {
                std::cout << "** " << "Parameter can be reconfigured dynamically" << std::endl;
            }
            else
            {
                std::cout << "** " << "Parameter cannot be reconfigured dynamically" << std::endl;
            }
        }
    }
}

TEST_F(ConfigurationTest, print_confluence_table)
{
    std::cout << "This documentation was generated with test " << test_info_->name() << std::endl
              << youtils::buildInfoString()
              << "Some parameters might not be documented - these should be regarded as internal and should *not* be changed"
              << std::endl;

    std::cout << "||component || key || default value || dynamically reconfigurable || remarks ||" << std::endl;

    for (const auto& info : initialized_params::ParameterInfo::get_parameter_info_list())
    {
        if (T(info->document_))
        {
            std::cout << " | " << info->section_name;
            std::cout << " | " << info->name;
            std::cout << " | " << (info->has_default ? (std::string("\"") +  info->def_  + "\"") : "---");
            std::cout << " | " << (info->can_be_reset ? "yes" : "no");
            std::cout << " | " << info->doc_string;
            std::cout << " | " << std::endl;
        }
    }
}

TEST_F(ConfigurationTest, print_framework_parameter_dict)
{
    std::map<std::string, std::list<std::string>> sections;
    sections["storagedriver"] =
    {
        "backend_connection_manager",
        "backend_garbage_collector",
        "content_addressed_cache",
        "event_publisher",
        "distributed_lock_store",
        "failovercache",
        "file_driver",
        "filesystem",
        "metadata_server",
        "scocache",
        "scrub_manager",
        "threadpool_component",
        "volume_manager",
        "volume_registry",
        "volume_router",
        "volume_router_cluster"
    };
    sections["metadataserver"] =
    {
        "backend_connection_manager",
        "metadata_server"
    };

    std::map<std::string, std::map<std::string, std::list<std::string>>> parameters;
    for (const auto& info : initialized_params::ParameterInfo::get_parameter_info_list())
    {
        if (T(info->document_))
        {
            parameters[info->section_name][info->has_default ? "optional" : "mandatory"].push_back(info->name);
        }
    }

    std::cout << "This documentation was generated with test " << test_info_->name() << std::endl
              << youtils::buildInfoString()
              << std::endl << std::endl;

    std::cout << "    parameters = {" << std::endl
              << "        # hg branch: " << BuildInfo::branch << std::endl
              << "        # hg revision: " << BuildInfo::revision << std::endl
              << "        # buildTime: " << BuildInfo::buildTime << std::endl;
    for (const auto& types: sections)
    {
        std::cout << "        '" << types.first << "': {" << std::endl;
        for (const auto& section: types.second)
        {
            const auto section_info = parameters[section];
            std::cout << "            '" << section << "': {" << std::endl;
            std::cout << "                'optional': [";
            auto it = section_info.find("optional");
            if (it != section_info.end())
            {
                for (const auto& entry: it->second)
                {
                    std::cout << "'" << entry << "', ";
                }
            }
            std::cout << "]," << std::endl;
            std::cout << "                'mandatory': [";
            it = section_info.find("mandatory");
            if (it != section_info.end())
            {
                for (const auto& entry: it->second)
                {
                    std::cout << "'" << entry << "', ";
                }
            }
            std::cout << "]" << std::endl;
            std::cout << "            }," << std::endl;
        }
        std::cout << "        }," << std::endl;
    }
    std::cout << "    }" << std::endl
              << std::endl << std::endl;
}

// Above text dict creation can be changed to this:
// Py_InitializeEx(0);
// using namespace boost::python;
// dict global_dict;
// static const str optional("optional");
// static const str mandatory("mandatory");
// for (const auto& info : initialized_params::ParameterInfo::get_parameter_info_list())
// {
//     if (T(info->document_))
//     {
//         dict section_dict = extract<dict>(global_dict.get(info->section_name, dict()))();

//         if(info->has_default)
//         {
//             boost::python::list = extract<list>(section_dict.get(optional, list()))
//                 ().append(info->name);
//         }
//         else
//         {
//             extract<list>(section_dict.get(mandatory, list()))().append(info->name);
//         }
//     }
// }
// auto mod = import("pprint");
// mod.attr("pprint")(global_dict);
// Py_Finalize();

}

// Local Variables: **
// mode: c++ **
// End: **
