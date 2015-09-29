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

#ifndef VFS_VIRTUAL_DISK_FORMAT_VMDK_H_
#define VFS_VIRTUAL_DISK_FORMAT_VMDK_H_

#include "VirtualDiskFormat.h"

namespace volumedriverfstest
{
class FileSystemTestBase;
}

namespace volumedriverfs
{

class FileSystem;

class VirtualDiskFormatVmdk final
    : public VirtualDiskFormat
{
    friend class volumedriverfstest::FileSystemTestBase;

public:
    VirtualDiskFormatVmdk();

    virtual ~VirtualDiskFormatVmdk() = default;

    virtual std::string
    name() const override final
    {
        return name_;
    }

    virtual void
    create_volume(FileSystem& fs,
                  const FrontendPath& path,
                  const volumedriver::VolumeId& volume_id,
                  VirtualDiskFormat::CreateFun fun) override final;

    static const std::string name_;

private:
    DECLARE_LOGGER("VirtualDiskFormatVmdk");

    FrontendPath
    make_volume_path_(const FrontendPath& vmdkpath);
};

}

#endif // !VFS_VIRTUAL_DISK_FORMAT_VMDK_H_
