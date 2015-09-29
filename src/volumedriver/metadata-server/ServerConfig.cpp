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

#include "ServerConfig.h"

#include <youtils/StreamUtils.h>

namespace metadata_server
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;

ServerConfig::ServerConfig(const volumedriver::MDSNodeConfig& ncfg,
                           const boost::filesystem::path& db,
                           const boost::filesystem::path& scratch)
    : node_config(ncfg)
    , db_path(db)
    , scratch_path(scratch)
{}

bool
ServerConfig::operator==(const ServerConfig& other) const
{
    return node_config == other.node_config and
        db_path == other.db_path and
        scratch_path == other.scratch_path;
}

bool
ServerConfig::operator!=(const ServerConfig& other) const
{
    return not operator==(other);
}

bool
ServerConfig::conflicts(const ServerConfig& other) const
{
    return node_config == other.node_config or
        db_path == other.db_path or
        scratch_path == other.scratch_path;
}

std::ostream&
operator<<(std::ostream& os,
           const ServerConfig& cfg)
{
    return os <<
        "(addr=" << cfg.node_config <<
        ",db=" << cfg.db_path <<
        ",scratch=" << cfg.scratch_path <<
        ")";
}

std::ostream&
operator<<(std::ostream& os,
           const ServerConfigs& cfgs)
{
    return youtils::StreamUtils::stream_out_sequence(os,
                                                     cfgs);
}

}

namespace initialized_params
{

namespace mds = metadata_server;

const std::string
PropertyTreeVectorAccessor<mds::ServerConfig>::db_path_key("db_directory");

const std::string
PropertyTreeVectorAccessor<mds::ServerConfig>::scratch_path_key("scratch_directory");

}
