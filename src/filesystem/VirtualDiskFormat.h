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
