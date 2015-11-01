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

#ifndef VD_MOUNTPOINT_CONFIG_H_
#define VD_MOUNTPOINT_CONFIG_H_

#include <iosfwd>
#include <vector>

#include <boost/filesystem.hpp>

#include <youtils/InitializedParam.h>

namespace volumedriver
{

struct MountPointConfig
{
    boost::filesystem::path path;
    uint64_t size;

    MountPointConfig(const boost::filesystem::path& p,
                     uint64_t s)
        : path(p)
        , size(s)
    {}

    ~MountPointConfig() = default;

    MountPointConfig(const MountPointConfig&) = default;

    MountPointConfig&
    operator=(const MountPointConfig&) = default;

    bool
    operator==(const MountPointConfig&) const;

    bool
    operator!=(const MountPointConfig&) const;
};

std::ostream&
operator<<(std::ostream& os,
           const MountPointConfig& cfg);

using MountPointConfigs = std::vector<MountPointConfig>;

std::ostream&
operator<<(std::ostream& os,
           const MountPointConfigs& cfgs);
}

namespace initialized_params
{

template<>
struct PropertyTreeVectorAccessor<volumedriver::MountPointConfig>
{
    static const std::string path_key;
    static const std::string size_key;

    // don't use fs::path here, as that leads to something like
    // "some_path": "\"\/foo\/bar\"" in the JSON output, but we do want the JSON
    // to be user editable.
    static volumedriver::MountPointConfig
    get(const boost::property_tree::ptree& pt)
    {
        boost::filesystem::path p(pt.get<std::string>(path_key));
        youtils::DimensionedValue s(pt.get<youtils::DimensionedValue>(size_key));

        return volumedriver::MountPointConfig(p, s.getBytes());
    }

    static void
    put(boost::property_tree::ptree& pt,
        const volumedriver::MountPointConfig& cfg)
    {
        pt.put(path_key,
               cfg.path.string());
        pt.put(size_key,
               youtils::DimensionedValue(cfg.size).toString());
    }
};

}

#endif // !VD_MOUNTPOINT_CONFIG_H_
