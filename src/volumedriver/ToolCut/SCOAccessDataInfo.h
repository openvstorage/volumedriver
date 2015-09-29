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

#ifndef SCO_ACCESS_DATA_INFO_H_
#define SCO_ACCESS_DATA_INFO_H_

#include <boost/python/list.hpp>
#include <boost/filesystem.hpp>

#include "../SCOAccessData.h"


namespace toolcut
{

namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace vd = volumedriver;

class SCOAccessDataInfo
{
public:
    explicit SCOAccessDataInfo(const fs::path& p);

    SCOAccessDataInfo(const fs::path& dst_path,
                      bpy::object& backend_config,
                      const std::string& nspace);

    SCOAccessDataInfo(const SCOAccessDataInfo&) = delete;

    SCOAccessDataInfo&
    operator=(const SCOAccessDataInfo&) = delete;

    std::string
    getNamespace() const;

    float
    getReadActivity() const;

    bpy::list
    getEntries() const;

private:
    DECLARE_LOGGER("SCOAccessDataInfo");

    vd::SCOAccessDataPtr sad_;
};

}
#endif //! SCO_ACCESS_DATA_INFO_H_

// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
