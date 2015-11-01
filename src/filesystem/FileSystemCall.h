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

#ifndef VFS_FILE_SYSTEM_CALL_H_
#define VFS_FILE_SYSTEM_CALL_H_

#include <iosfwd>
#include <string>

#include <boost/optional.hpp>

#include <youtils/Assert.h>

namespace volumedriverfs
{

enum class FileSystemCall
{
    Getattr,
    Access,
    Readlink,
    Readdir,
    Mknod,
    Mkdir,
    Unlink,
    Rmdir,
    Symlink,
    Rename,
    Link,
    Chown,
    Chmod,
    Truncate,
    Open,
    Read,
    Write,
    Statfs,
    Utimens,
    Release,
    Fsync,
    Opendir,
    Releasedir
};

typedef boost::optional<FileSystemCall> MaybeFileSystemCall;

std::ostream&
operator<<(std::ostream& os, const FileSystemCall call);

std::istream&
operator>>(std::istream& is, MaybeFileSystemCall& call);

}

#endif // !VFS_FILE_SYSTEM_CALL_H_

// Local Variables: **
// mode: c++ **
// End: **
