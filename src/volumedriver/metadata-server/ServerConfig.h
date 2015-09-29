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

#ifndef MDS_SERVER_CONFIG_H_
#define MDS_SERVER_CONFIG_H_

#include "../MDSNodeConfig.h"

#include <youtils/InitializedParam.h>

namespace metadata_server
{

struct ServerConfig
{
    ServerConfig(const volumedriver::MDSNodeConfig& ncfg,
                 const boost::filesystem::path& db,
                 const boost::filesystem::path& scratch);

    ~ServerConfig() = default;

    ServerConfig(const ServerConfig&) = default;

    ServerConfig&
    operator=(const ServerConfig&) = default;

    bool
    operator==(const ServerConfig&) const;

    bool
    operator!=(const ServerConfig&) const;

    bool
    conflicts(const ServerConfig&) const;

    volumedriver::MDSNodeConfig node_config;
    boost::filesystem::path db_path;
    boost::filesystem::path scratch_path;
};

using ServerConfigs = std::vector<ServerConfig>;

std::ostream&
operator<<(std::ostream& os,
           const ServerConfig& cfg);

std::ostream&
operator<<(std::ostream& os,
           const ServerConfigs& cfgs);

}

namespace initialized_params
{

template<>
struct PropertyTreeVectorAccessor<metadata_server::ServerConfig>
{
    using Type = metadata_server::ServerConfig;

    static const std::string db_path_key;
    static const std::string scratch_path_key;

    static Type
    get(const boost::property_tree::ptree& pt)
    {
        const auto
            ncfg(PropertyTreeVectorAccessor<volumedriver::MDSNodeConfig>::get(pt));
        return Type(ncfg,
                    pt.get<boost::filesystem::path>(db_path_key),
                    pt.get<boost::filesystem::path>(scratch_path_key));
    }

    static void
    put(boost::property_tree::ptree& pt,
        const Type& cfg)
    {
        PropertyTreeVectorAccessor<volumedriver::MDSNodeConfig>::put(pt,
                                                                     cfg.node_config);
        pt.put(db_path_key, cfg.db_path);
        pt.put(scratch_path_key, cfg.scratch_path);
    }
};

}

#endif //!MDS_SERVER_CONFIG_H_
