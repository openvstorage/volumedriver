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
