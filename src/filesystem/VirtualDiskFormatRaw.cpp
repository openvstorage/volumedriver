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

#include "FileSystem.h"
#include "VirtualDiskFormatRaw.h"

#include <boost/filesystem.hpp>

namespace volumedriverfs
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

const std::string VirtualDiskFormatRaw::name_("raw");

void
VirtualDiskFormatRaw::create_volume(FileSystem& /*fs*/,
                                    const FrontendPath& path,
                                    const vd::VolumeId& /* volume_id */,
                                    VirtualDiskFormat::CreateFun create_fun)
{
    THROW_UNLESS(is_volume_path(path));
    create_fun(path);
}

}
