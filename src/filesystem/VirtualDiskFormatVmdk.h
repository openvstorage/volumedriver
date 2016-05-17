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
