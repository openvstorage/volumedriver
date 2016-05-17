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

#include "BuildInfo.h"
#include "PythonBuildInfo.h"

#include <string>

#include <boost/python/class.hpp>

namespace youtils
{

namespace python
{

namespace bpy = boost::python;

std::string
BuildInfo::revision()
{
    return ::BuildInfo::revision;
}

std::string
BuildInfo::branch()
{
    return ::BuildInfo::branch;
}

std::string
BuildInfo::timestamp()
{
    return ::BuildInfo::buildTime;
}

void
BuildInfo::registerize()
{
    bpy::class_<BuildInfo,
                boost::noncopyable>("BuildInfo",
                                    "Holds information about this build",
                                    bpy::no_init)
        .def("revision", &BuildInfo::revision,
             "Get the revision of this build"
             "@result the revision of this build, a string")
        .staticmethod("revision")
        .def("branch", &BuildInfo::branch,
             "Get the branch of this build"
             "@result the branch of this build, a string")
        .staticmethod("branch")
        .def("timestamp", &BuildInfo::timestamp,
             "Get the build time version of this build"
             "@result the build time version of this build, a string")
        .staticmethod("timestamp")
        ;
}

}

}
