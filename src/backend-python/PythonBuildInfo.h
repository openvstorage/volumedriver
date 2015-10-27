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
