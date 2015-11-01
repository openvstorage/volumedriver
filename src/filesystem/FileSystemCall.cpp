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
