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
VolumeDriverComponent::read_config(std::stringstream& ss)
{
    LOG_INFO("Configuration passed:\n" << ss.str());

    bpt::ptree pt;
    bpt::json_parser::read_json(ss,
                                pt);

    VolumeDriverComponent::verify_property_tree(pt);

    return pt;
}

bpt::ptree
VolumeDriverComponent::read_config_file(const boost::filesystem::path& file)
{
    fs::ifstream ifs(file);
    if (not ifs)
    {
        int err = errno;
        LOG_ERROR("Failed to open " << file << ": " << strerror(err));
        throw fungi::IOException("Failed to open config file",
                                 file.string().c_str());
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    return read_config(ss);
}

}

// Local Variables: **
// mode: c++ **
// End: **
