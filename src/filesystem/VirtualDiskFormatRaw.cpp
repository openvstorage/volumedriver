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
