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

#include "BuildInfoAdapter.h"
#include "../BuildInfo.h"

#include <string>

#include <boost/python/class.hpp>

namespace youtils
{

namespace python
{

namespace bpy = boost::python;

namespace
{

std::string
revision()
{
    return BuildInfo::revision;
}

std::string
branch()
{
    return BuildInfo::branch;
}

std::string
timestamp()
{
    return BuildInfo::buildTime;
}

std::string
repository_url()
{
    return BuildInfo::repository_url;
}

std::string
version_revision()
{
    return BuildInfo::version_revision;
}

}

DEFINE_PYTHON_WRAPPER(BuildInfoAdapter)
{
    bpy::class_<BuildInfo,
                boost::noncopyable>("BuildInfo",
                                    "Holds information about this build",
                                    bpy::no_init)
        .def("revision",
             &revision,
             "Get the revision of this build"
             "@result the revision of this build, a string")
        .staticmethod("revision")
        .def("version_revision",
             &version_revision,
             "Get the version revision of this build"
             "@result the version revision of this build, a string")
        .staticmethod("version_revision")
        .def("branch",
             &branch,
             "Get the branch of this build"
             "@result the branch of this build, a string")
        .staticmethod("branch")
        .def("timestamp",
             &timestamp,
             "Get the build time version of this build"
             "@result the build time version of this build, a string")
        .staticmethod("timestamp")
        .def("repository_url",
             &repository_url,
             "Get the URL of the repository the code was taken from"
             "@result the repository URL, a string")
        .staticmethod("repository_url")
        ;
}

}

}
