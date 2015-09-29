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

#include "AlbaConfig.h"
#include "BackendConfig.h"
#include "BackendParameters.h"
#include "LocalConfig.h"
#include "MultiConfig.h"
#include "S3Config.h"

#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <youtils/Assert.h>

namespace backend
{
namespace fs = boost::filesystem;
namespace ip = initialized_params;

std::unique_ptr<BackendConfig>
BackendConfig::makeBackendConfig(const std::string& json_string)
{
    boost::property_tree::ptree pt;
    std::stringstream ss(json_string);

    boost::property_tree::json_parser::read_json(ss,
                                                 pt);
    return makeBackendConfig(pt);
}

std::unique_ptr<BackendConfig>
BackendConfig::makeBackendConfig(const fs::path& json_file)
{
    boost::property_tree::ptree pt;
    boost::property_tree::json_parser::read_json(json_file.string(),
                                                 pt);
    return makeBackendConfig(pt);
}

std::unique_ptr<BackendConfig>
BackendConfig::makeBackendConfig(const boost::property_tree::ptree& pt)
{
    ip::PARAMETER_TYPE(backend_type) bt(pt);

    switch(bt.value())
    {
    case BackendType::LOCAL :
        return std::unique_ptr<BackendConfig>(new LocalConfig(pt));
    case BackendType::S3:
        return std::unique_ptr<BackendConfig>(new S3Config(pt));
    case BackendType::MULTI:
        return std::unique_ptr<BackendConfig>(new MultiConfig(pt));
    case BackendType::ALBA:
        return std::unique_ptr<BackendConfig>(new AlbaConfig(pt));
    }
    UNREACHABLE
}

const char*
BackendConfig::componentName() const
{
    return ip::backend_connection_manager_name;
}

std::string
BackendConfig::sectionName(const std::string& section)
{
    return std::string(ip::backend_connection_manager_name) + "." + section;
}

}

// Local Variables: **
// mode: c++ **
// End: **
