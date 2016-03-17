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

#ifndef VFS_VIRTUAL_DISK_FORMAT_H_
#define VFS_VIRTUAL_DISK_FORMAT_H_

#include "FrontendPath.h"
#include "Object.h"

#include <functional>
#include <string>

#include <boost/regex.hpp>

namespace volumedriverfs
{

class FileSystem;

// The angels are weeping. I'm sorry.
class VirtualDiskFormat
{
public:
    VirtualDiskFormat(const std::string& volume_suffix);

    virtual ~VirtualDiskFormat()
    {}

    virtual std::string
    name() const = 0;

    std::string
    volume_suffix() const
    {
        return volume_suffix_;
    }

    bool
    is_volume_path(const FrontendPath& p) const
    {
        return boost::regex_match(p.str(), volume_regex_);
    }

    typedef std::function<void(const FrontendPath&)> CreateFun;

    virtual void
    create_volume(FileSystem& fs,
                  const FrontendPath& path,
                  const volumedriver::VolumeId& volume_id,
                  CreateFun create_fun) = 0;

protected:
    const std::string volume_suffix_;
    const boost::regex volume_regex_;
};

}

#endif // !VFS_VIRTUAL_DISK_FORMAT_H_
