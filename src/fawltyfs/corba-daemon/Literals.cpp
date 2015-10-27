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

#include "Literals.h"
fawltyfs::FileSystemCall
fromCorba(const Fawlty::FileSystemCall in)
{
    switch(in)
    {
    case Fawlty::FileSystemCall::Getattr:
        return fawltyfs::FileSystemCall::Getattr;
    case Fawlty::FileSystemCall::Access:
        return fawltyfs::FileSystemCall::Access;
    case Fawlty::FileSystemCall::Readlink:
        return fawltyfs::FileSystemCall::Readlink;
    case Fawlty::FileSystemCall::Readdir:
        return fawltyfs::FileSystemCall::Readdir;
    case Fawlty::FileSystemCall::Mknod:
        return fawltyfs::FileSystemCall::Mknod;
    case Fawlty::FileSystemCall::Mkdir:
        return fawltyfs::FileSystemCall::Mkdir;
    case Fawlty::FileSystemCall::Unlink:
        return fawltyfs::FileSystemCall::Unlink;
    case Fawlty::FileSystemCall::Rmdir:
        return fawltyfs::FileSystemCall::Rmdir;
    case Fawlty::FileSystemCall::Symlink:
        return fawltyfs::FileSystemCall::Symlink;
    case Fawlty::FileSystemCall::Rename:
        return fawltyfs::FileSystemCall::Rename;
    case Fawlty::FileSystemCall::Link:
        return fawltyfs::FileSystemCall::Link;
    case Fawlty::FileSystemCall::Chown:
        return fawltyfs::FileSystemCall::Chown;
    case Fawlty::FileSystemCall::Chmod:
        return fawltyfs::FileSystemCall::Chmod;
    case Fawlty::FileSystemCall::Truncate:
        return fawltyfs::FileSystemCall::Truncate;
    case Fawlty::FileSystemCall::Open:
        return fawltyfs::FileSystemCall::Open;
    case Fawlty::FileSystemCall::Read:
        return fawltyfs::FileSystemCall::Read;
    case Fawlty::FileSystemCall::Write:
        return fawltyfs::FileSystemCall::Write;
    case Fawlty::FileSystemCall::Statfs:
        return fawltyfs::FileSystemCall::Statfs;
    case Fawlty::FileSystemCall::Utimens:
        return fawltyfs::FileSystemCall::Utimens;
    case Fawlty::FileSystemCall::Release:
        return fawltyfs::FileSystemCall::Release;
    case Fawlty::FileSystemCall::Fsync:
        return fawltyfs::FileSystemCall::Fsync;
    case Fawlty::FileSystemCall::Opendir:
        return fawltyfs::FileSystemCall::Opendir;
    case Fawlty::FileSystemCall::Releasedir:
        return fawltyfs::FileSystemCall::Releasedir;
        // Just here to make the compiler happy
    default:
        return fawltyfs::FileSystemCall::Releasedir;
    }
}

Fawlty::FileSystemCall
toCorba(fawltyfs::FileSystemCall in)
{
    switch(in)
    {
    case fawltyfs::FileSystemCall::Getattr:
        return Fawlty::FileSystemCall::Getattr;
    case fawltyfs::FileSystemCall::Access:
        return Fawlty::FileSystemCall::Access;
    case fawltyfs::FileSystemCall::Readlink:
        return Fawlty::FileSystemCall::Readlink;
    case fawltyfs::FileSystemCall::Readdir:
        return Fawlty::FileSystemCall::Readdir;
    case fawltyfs::FileSystemCall::Mknod:
        return Fawlty::FileSystemCall::Mknod;
    case fawltyfs::FileSystemCall::Mkdir:
        return Fawlty::FileSystemCall::Mkdir;
    case fawltyfs::FileSystemCall::Unlink:
        return Fawlty::FileSystemCall::Unlink;
    case fawltyfs::FileSystemCall::Rmdir:
        return Fawlty::FileSystemCall::Rmdir;
    case fawltyfs::FileSystemCall::Symlink:
        return Fawlty::FileSystemCall::Symlink;
    case fawltyfs::FileSystemCall::Rename:
        return Fawlty::FileSystemCall::Rename;
    case fawltyfs::FileSystemCall::Link:
        return Fawlty::FileSystemCall::Link;
    case fawltyfs::FileSystemCall::Chown:
        return Fawlty::FileSystemCall::Chown;
    case fawltyfs::FileSystemCall::Chmod:
        return Fawlty::FileSystemCall::Chmod;
    case fawltyfs::FileSystemCall::Truncate:
        return Fawlty::FileSystemCall::Truncate;
    case fawltyfs::FileSystemCall::Open:
        return Fawlty::FileSystemCall::Open;
    case fawltyfs::FileSystemCall::Read:
        return Fawlty::FileSystemCall::Read;
    case fawltyfs::FileSystemCall::Write:
        return Fawlty::FileSystemCall::Write;
    case fawltyfs::FileSystemCall::Statfs:
        return Fawlty::FileSystemCall::Statfs;
    case fawltyfs::FileSystemCall::Utimens:
        return Fawlty::FileSystemCall::Utimens;
    case fawltyfs::FileSystemCall::Release:
        return Fawlty::FileSystemCall::Release;
    case fawltyfs::FileSystemCall::Fsync:
        return Fawlty::FileSystemCall::Fsync;
    case fawltyfs::FileSystemCall::Opendir:
        return Fawlty::FileSystemCall::Opendir;
    case fawltyfs::FileSystemCall::Releasedir:
        return Fawlty::FileSystemCall::Releasedir;
        // Just here to make the compiler happy
    default:
        return Fawlty::FileSystemCall::Releasedir;
    }

}


std::set<fawltyfs::FileSystemCall>
fromCorba(const Fawlty::FileSystemCalls& calls)
{
    std::set<fawltyfs::FileSystemCall> ret;
    for(unsigned i = 0; i < calls.length(); ++i)
    {
        ret.insert(fromCorba(calls[i]));
    }

    return ret;
}

Fawlty::FileSystemCalls
toCorba(const std::set<fawltyfs::FileSystemCall>& calls)
{
    Fawlty::FileSystemCalls ret;
    ret.length(calls.size());

    int i = 0;

    for(fawltyfs::FileSystemCall call : calls)
    {
        ret[i] = toCorba(call);
        ++i;
    }

    return ret;
}
