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

#include "Extent.h"
#include <youtils/Assert.h>
#include <youtils/FileDescriptor.h>

namespace filedriver
{

namespace fs = boost::filesystem;
namespace yt = youtils;

Extent::Extent(const fs::path& p)
    : path(p)
{
    LOG_TRACE(path);
}

size_t
Extent::read(off_t off,
             void* buf,
             size_t bufsize)
{
    LOG_TRACE(path << ": off " << off << ", size " << bufsize);

    VERIFY(off + bufsize <= capacity());
    yt::FileDescriptor sio(path,
                     yt::FDMode::Read);
    return sio.pread(buf, bufsize, off);
}

size_t
Extent::write(off_t off,
              const void* buf,
              size_t bufsize)
{
    LOG_TRACE(path << ": off " << off << ", size " << bufsize);

    VERIFY(off + bufsize <= capacity());
    yt::FileDescriptor sio(path,
                     yt::FDMode::Write,
                     CreateIfNecessary::T);

    return sio.pwrite(buf, bufsize, off);
}

void
Extent::resize(uint64_t size)
{
    LOG_TRACE(path << ": size " << size);

    VERIFY(size > 0);
    VERIFY(size <= capacity());

    yt::FileDescriptor sio(path,
                     yt::FDMode::Write,
                     CreateIfNecessary::T);
    sio.truncate(size);
}

size_t
Extent::size()
{
    LOG_TRACE(path);

    yt::FileDescriptor sio(path,
                     yt::FDMode::Read);

    size_t res = sio.size();
    LOG_TRACE(path << ": size " << res);

    ASSERT(res > 0);
    ASSERT(res <= capacity());

    return res;
}

}
