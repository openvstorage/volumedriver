// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
