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
