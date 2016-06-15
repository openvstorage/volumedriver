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

#ifndef FILE_SYSTEM_CALL_H_
#define FILE_SYSTEM_CALL_H_

#include <string>
#include <youtils/Assert.h>

namespace fawltyfs
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

inline std::ostream&
operator<<(std::ostream& strm, const FileSystemCall call)
{
    switch (call)
    {

    case FileSystemCall::Getattr:
        return strm << "Getattr";

    case FileSystemCall::Access:
        return strm << "Access";

    case FileSystemCall::Readlink:
        return strm << "Readlink";

    case FileSystemCall::Readdir:
        return strm << "Readdir";

    case FileSystemCall::Mknod:
        return strm << "Mknod";

    case FileSystemCall::Mkdir:
        return strm << "Mkdir";

    case FileSystemCall::Unlink:
        return strm << "Unlink";

    case FileSystemCall::Rmdir:
        return strm << "Rmdir";

    case FileSystemCall::Symlink:
        return strm << "Symlink";

    case FileSystemCall::Rename:
        return strm << "Rename";

    case FileSystemCall::Link:
        return strm << "Link";

    case FileSystemCall::Chown:
        return strm << "Chown";

    case FileSystemCall::Chmod:
        return strm << "Chmod";

    case FileSystemCall::Truncate:
        return strm << "Truncate";

    case FileSystemCall::Open:
        return strm << "Open";

    case FileSystemCall::Read:
        return strm << "Read";

    case FileSystemCall::Write:
        return strm << "Write";

    case FileSystemCall::Statfs:
        return strm << "Statfs";

    case FileSystemCall::Utimens:
        return strm << "Utimens";

    case FileSystemCall::Release:
        return strm << "Release";

    case FileSystemCall::Fsync:
        return strm << "Fsync";

    case FileSystemCall::Opendir:
        return strm << "Opendir";

    case FileSystemCall::Releasedir:
        return strm << "Releasedir";
    }
    UNREACHABLE;

}

}

#endif // !FILE_SYSTEM_CALL_H_

// Local Variables: **
// compile-command: "scons -D -j 4" **
// mode: c++ **
// End: **
