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
VolumeDriverComponent::read_config(std::stringstream& ss,
                                   VerifyConfig verify_config)
{
    LOG_INFO("Configuration passed:\n" << ss.str());

    bpt::ptree pt;
    bpt::json_parser::read_json(ss,
                                pt);

    if (verify_config == VerifyConfig::T)
    {
        VolumeDriverComponent::verify_property_tree(pt);
    }

    return pt;
}

bpt::ptree
VolumeDriverComponent::read_config_file(const boost::filesystem::path& file,
                                        VerifyConfig verify_config)
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
    return read_config(ss,
                       verify_config);
}

}

// Local Variables: **
// mode: c++ **
// End: **
