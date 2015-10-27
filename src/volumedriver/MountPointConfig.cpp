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

#include "MountPointConfig.h"

namespace volumedriver
{

bool
MountPointConfig::operator==(const MountPointConfig& other) const
{
    return path == other.path and
        size == other.size;
}

bool
MountPointConfig::operator!=(const MountPointConfig& other) const
{
    return not operator==(other);
}

std::ostream&
operator<<(std::ostream& os,
           const MountPointConfig& cfg)
{
    return os << "(" << cfg.path << ", " << cfg.size << ")";
}

std::ostream&
operator<<(std::ostream& os,
           const MountPointConfigs& cfgs)
{
    bool sep = false;

    os << "[";

    for (const auto& cfg : cfgs)
    {
        if (sep)
        {
            os << ", ";
        }
        else
        {
            sep = true;
        }

        os << cfg;
    }

    os << "]";

    return os;
}

}

namespace initialized_params
{

namespace vd = volumedriver;

const std::string
PropertyTreeVectorAccessor<vd::MountPointConfig>::path_key("path");

const std::string
PropertyTreeVectorAccessor<vd::MountPointConfig>::size_key("size");

}
