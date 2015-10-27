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

#include "VolumeDriverComponent.h"
#include "InitializedParam.h"
#include "Notifier.h"

#include <boost/filesystem/fstream.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace youtils
{

namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace ip = initialized_params;

VolumeDriverComponent::VolumeDriverComponent(const RegisterComponent registerize,
                                             const bpt::ptree&  /*pt*/)
    : registered_(registerize)
{
    if(registered_ == RegisterComponent::T)
    {
        Notifier::registerComponent(this);
    }
}

VolumeDriverComponent::~VolumeDriverComponent()
{
    if(registered_ == RegisterComponent::T)
    {
        Notifier::removeComponent(this);
    }
}

void
VolumeDriverComponent::verify_property_tree(const bpt::ptree& ptree)
{
    initialized_params::ParameterInfo::verify_property_tree(ptree);
}

bpt::ptree
VolumeDriverComponent::read_config_file(const boost::filesystem::path& file)
{
    fs::ifstream ifs(file);
    if (ifs)
    {
        std::stringstream ss;
        ss << ifs.rdbuf();
        LOG_INFO("Configuration passed:\n" << ss.str());
    }

    bpt::ptree pt;
    bpt::json_parser::read_json(file.string(),
                                pt);

    verify_property_tree(pt);

    return pt;
}

}

// Local Variables: **
// mode: c++ **
// End: **
