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

#include "VirtualDiskFormatVmdk.h"
#include "FileSystem.h"

#include <youtils/ScopeExit.h>

namespace volumedriverfs
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

const std::string VirtualDiskFormatVmdk::name_("vmdk");

namespace
{

std::string
create_vmdk_contents(const FrontendPath& path, const uint64_t size_in_bytes)
{
    // Only the volume filename is serialized, otherwize VMware is confused as
    // path is an absolute path which starts at the datastore.
    // WARNING the .vmdk needs to contain quotes around the filename,
    // currently these are added by fs::path automagically.

    //TODO [bdv] need to check here whether parent size was sector aligned?
    uint64_t size_in_sectors = size_in_bytes / 512;

    std::stringstream ss;
    ss <<
        "# Disk DescriptorFile\n"
        "version=1\n"
        "encoding=\"UTF-8\"\n"
        "CID=fffffffe\n"
        "parentCID=ffffffff\n"
        "isNativeSnapshot=\"no\"\n"
        "createType=\"vmfs\"\n\n"
        "# Extent description\n"
        "RW " << size_in_sectors << " VMFS " << fs::path(path.str()).filename() << "\n";

    return ss.str();
}

}

VirtualDiskFormatVmdk::VirtualDiskFormatVmdk()
    : VirtualDiskFormat("-flat.vmdk")
{}

FrontendPath
VirtualDiskFormatVmdk::make_volume_path_(const FrontendPath& vmdk_path)
{
    const fs::path p(vmdk_path.str());
    return FrontendPath((p.parent_path() / p.stem()).string() + volume_suffix_);
}

void
VirtualDiskFormatVmdk::create_volume(FileSystem& fs,
                                     const FrontendPath& path,
                                     const vd::VolumeId& id,
                                     VirtualDiskFormat::CreateFun create_fun)
{
    // TODO:
    // There's no checking whether path has a .vmdk suffix. Not that it matters at all
    // to the filesystem code, but since the responsibility of creating the file content
    // and a volume with a -flat.vmdk suffix was inflicted on the filesystem we
    // might want to be more strict on what we accept?
    LOG_TRACE("path " << path << ", id " << id);

    const FrontendPath tmp(path.str() + std::string(".") + yt::UUID().str());

    fs.mknod(tmp,
             UserId(::getuid()),
             GroupId(::getgid()),
             Permissions(S_IFREG bitor S_IRUSR bitor S_IWUSR));

    try
    {
        const FrontendPath vpath(make_volume_path_(path));
        create_fun(vpath);

        const uint64_t size = fs.object_router().get_size(ObjectId(id.str()));

        Handle::Ptr h;
        fs.open(tmp, O_WRONLY, h);

        LOG_TRACE("Creating VMDK for id " << id << " @ " << path << ", volume: " << vpath);
        {
            auto exit(yt::make_scope_exit([&fs, &h, &vpath]
                                          {
                                              fs.release(vpath,
                                                         std::move(h));
                                          }));

            const std::string wbuf(create_vmdk_contents(vpath, size));
            size_t size = wbuf.size();
            fs.write(*h,
                     size,
                     wbuf.c_str(),
                     0);

            if (size != wbuf.size())
            {
                LOG_ERROR("Wrote less (" << size << ") than expected (" <<
                          wbuf.size() << ") to temp file");
                throw fungi::IOException("Wrote less than expected to temp file",
                                         tmp.str().c_str());
            }
        }

        fs.rename(tmp,
                  path);
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Failed to create volume: " << EWHAT);

            fs.unlink(tmp);
            throw;
        });
}

}
