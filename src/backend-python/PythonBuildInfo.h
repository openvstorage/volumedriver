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

#ifndef BACKEND_PYTHON_BUILD_INFO_H_
#define BACKEND_PYTHON_BUILD_INFO_H_

#include <boost/python/object.hpp>

#include <youtils/BuildInfo.h>
#include <youtils/IOException.h>

namespace pythonbackend
{

class PythonBuildInfo
{
private:
    PythonBuildInfo() = delete;

    ~PythonBuildInfo() = delete;

public:
    MAKE_EXCEPTION(VersionNotSupportedError, fungi::IOException);

    static const std::string
    revision()
    {
        return BuildInfo::revision;
    }

    static const std::string
    branch()
    {
        return BuildInfo::branch;
    }

    static const std::string
    buildTime()
    {
        return BuildInfo::buildTime;
    }
};

}

#endif // BACKEND_PYTHON_BUILD_INFO_H_

// Local Variables: **
// mode: c++ **
// End: **
