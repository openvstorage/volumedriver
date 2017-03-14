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

#include "BuildInfoString.h"
#include "BuildInfo.h"
#include <sstream>

namespace youtils
{

std::string
buildInfoString()
{
    std::stringstream ss;

    ss << "version: " << BuildInfo::version << std::endl
       << "version revision: " << BuildInfo::version_revision << std::endl
       << "repository URL: " << BuildInfo::repository_url << std::endl
       << "branch: " << BuildInfo::branch << std::endl
#ifdef ENABLE_MD5_HASH
       << "md5 support: yes" <<  std::endl
#else
       << "md5 support: no" <<  std::endl
#endif
       << "revision: " << BuildInfo::revision << std::endl
       << "build time: " << BuildInfo::buildTime << std::endl;

    return ss.str();
}

}

// Local Variables: **
// mode: c++ **
// End: **
