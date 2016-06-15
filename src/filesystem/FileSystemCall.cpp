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

#include "FileSystemCall.h"

#include <iostream>

namespace volumedriverfs
{

namespace
{
DECLARE_LOGGER("FileSystemCall");
}

// XXX: unify - e.g. by using a boost::bimap? But we'd also like to have static checks.
std::ostream&
operator<<(std::ostream& os, const FileSystemCall call)
{
    switch (call)
    {
#define CASE(c)                                    \
        case FileSystemCall::c:                    \
            return os << #c

        CASE(Getattr);
        CASE(Access);
        CASE(Readlink);
        CASE(Readdir);
        CASE(Mknod);
        CASE(Mkdir);
        CASE(Unlink);
        CASE(Rmdir);
        CASE(Symlink);
        CASE(Rename);
        CASE(Link);
        CASE(Chown);
        CASE(Chmod);
        CASE(Truncate);
        CASE(Open);
        CASE(Read);
        CASE(Write);
        CASE(Statfs);
        CASE(Utimens);
        CASE(Release);
        CASE(Fsync);
        CASE(Opendir);
        CASE(Releasedir);

#undef CASE
    }
    UNREACHABLE;
}

std::istream&
operator>>(std::istream& is, MaybeFileSystemCall& call)
{
    std::string s;
    is >> s;

    if (is.bad() or is.fail())
    {
        LOG_ERROR("Cannot read string from istream");
        return is;
    }

#define CHECK(c)                                \
    if (s == #c)                                \
    {                                           \
        call = FileSystemCall::c;               \
        return is;                              \
    }

    CHECK(Getattr);
    CHECK(Access);
    CHECK(Readlink);
    CHECK(Readdir);
    CHECK(Mknod);
    CHECK(Mkdir);
    CHECK(Unlink);
    CHECK(Rmdir);
    CHECK(Symlink);
    CHECK(Rename);
    CHECK(Link);
    CHECK(Chown);
    CHECK(Chmod);
    CHECK(Truncate);
    CHECK(Open);
    CHECK(Read);
    CHECK(Write);
    CHECK(Statfs);
    CHECK(Utimens);
    CHECK(Release);
    CHECK(Fsync);
    CHECK(Opendir);
    CHECK(Releasedir);

#undef CHECK

    LOG_ERROR("Could not parse " << s << " as a FileSystemCall");
    is.setstate(std::ios::failbit);

    return is;
}

}
