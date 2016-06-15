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
