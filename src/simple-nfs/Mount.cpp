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

#include "File.h"
#include "Mount.h"

#include <sstream>
#include <sys/stat.h>
#include <boost/scope_exit.hpp>

extern "C"
{
#include <nfsc/libnfs.h>
}

namespace simple_nfs
{

namespace fs = boost::filesystem;
namespace yt = youtils;

namespace
{

struct CtxDeleter
{
    void
    operator()(nfs_context* ctx)
    {
        if (ctx)
        {
            nfs_destroy_context(ctx);
        }
    }
};

}

Mount::Mount(const std::string& server,
             const fs::path& xprt)
    : ctx_(nfs_init_context(), CtxDeleter())
    , server_(server)
    , export_(xprt)
{
    if (ctx_ == nullptr)
    {
        LOG_ERROR(share_name() << ": failed to init nfs context");
        throw Exception("Failed to init nfs_context",
                             share_name().c_str());
    }

    convert_errors_<const char*,
                    const char*>("mount",
                                 &nfs_mount,
                                 server_.c_str(),
                                 export_.string().c_str());
}

const char*
Mount::last_error()
{
    return nfs_get_error(ctx_.get());
}

template<typename... A>
void
Mount::convert_errors_(const char* desc,
                       int (*nfs_fun)(nfs_context*,
                                      A... args),
                       A... args)
{
    LOG_TRACE(desc);

    const int ret = (*nfs_fun)(ctx_.get(), args...);
    if (ret < 0)
    {
        std::stringstream ss;
        ss << desc << " failed: " << last_error();
        LOG_ERROR(ss.str());
        throw Exception(ss.str().c_str());
    }
}

void
Mount::mkdir(const fs::path& p)
{
    LOG_TRACE(p);
    convert_errors_<const char*>("mkdir",
                                 &nfs_mkdir,
                                 p.string().c_str());
}

void
Mount::rmdir(const fs::path& p)
{
    LOG_TRACE(p);
    convert_errors_<const char*>("rmdir",
                                 &nfs_rmdir,
                                 p.string().c_str());
}

namespace
{

bool
disregard_dir_entry(const std::string& str)
{
    return str == "." or str == "..";
}

}

std::list<DirectoryEntry>
Mount::readdir(const fs::path& p)
{
    nfsdir* dir;
    convert_errors_<const char*,
                    nfsdir**>("opendir",
                              &nfs_opendir,
                              p.string().c_str(),
                              &dir);

    BOOST_SCOPE_EXIT((ctx_)(&dir))
    {
        nfs_closedir(ctx_.get(), dir);
    }
    BOOST_SCOPE_EXIT_END;

    nfsdirent *dirent;

    std::list<DirectoryEntry> l;

    while ((dirent = nfs_readdir(ctx_.get(), dir)))
    {
        if (not disregard_dir_entry(dirent->name))
        {
            l.emplace_back(dirent->name,
                           dirent->inode,
                           dirent->type,
                           dirent->mode,
                           dirent->size,
                           dirent->uid,
                           dirent->gid);
        }
    }

    return l;
}

void
Mount::unlink(const fs::path& p)
{
    LOG_TRACE(p);

    convert_errors_<const char*>("unlink",
                                 &nfs_unlink,
                                 p.string().c_str());
}

void
Mount::rename(const fs::path& from,
              const fs::path& to)
{
    LOG_TRACE(from << " -> " << to);

    convert_errors_<const char*,
                    const char*>("rename",
                                 &nfs_rename,
                                 from.string().c_str(),
                                 to.string().c_str());
}

void
Mount::truncate(const fs::path& p,
                uint64_t len)
{
    LOG_TRACE(p << ", len " << len);

    convert_errors_<const char*,
                    uint64_t>("truncate",
                              &nfs_truncate,
                              p.string().c_str(),
                              len);
}

File
Mount::open(const fs::path& p,
            yt::FDMode mode,
            CreateIfNecessary create_if_necessary)
{
    LOG_TRACE(p);
    return File(*this, p, mode, create_if_necessary);
}

void
Mount::chmod(const boost::filesystem::path& p,
             int mode)
{
    LOG_TRACE(p);
    convert_errors_<const char*,
                    int>("chmod",
                         &nfs_chmod,
                         p.string().c_str(),
                         mode);
}

struct stat
Mount::stat(const boost::filesystem::path& p)
{
    LOG_TRACE(p);

    struct stat st;
    convert_errors_<const char*,
                    struct stat*>("stat",
                                  &nfs_stat,
                                  p.string().c_str(),
                                  &st);

    return st;
}

void
Mount::utimes(const boost::filesystem::path& p, struct timeval *times)
{
    LOG_TRACE(p);

    convert_errors_<const char *,
                    struct timeval*>("utimes",
                                     &nfs_utimes,
                                     p.string().c_str(),
                                     times);
}

int
Mount::get_fd()
{
    return nfs_get_fd(ctx_.get());
}

int
Mount::which_events()
{
    return nfs_which_events(ctx_.get());
}

void
Mount::handle_events(int revents)
{
    convert_errors_<int>("handle_events",
                         &nfs_service,
                         revents);
}

}
