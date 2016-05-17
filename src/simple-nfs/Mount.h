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

#ifndef SIMPLE_NFS_MOUNT_H_
#define SIMPLE_NFS_MOUNT_H_

#include "DirectoryEntry.h"

#include <list>
#include <memory>
#include <string>

#include <boost/filesystem.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/FileDescriptor.h>

struct nfs_context;

namespace simple_nfs
{

class File;

MAKE_EXCEPTION(Exception, fungi::IOException);

class Mount
{
public:
    Mount(const std::string& server,
          const boost::filesystem::path& xprt);

    ~Mount() = default;

    Mount(const Mount&) = default;

    Mount&
    operator=(const Mount&) = default;

    const std::string
    share_name() const
    {
        return std::string(server_) + std::string(":") + std::string(export_.string());
    }

    const char*
    last_error();

    void
    mkdir(const boost::filesystem::path& p);

    void
    rmdir(const boost::filesystem::path& p);

    std::list<DirectoryEntry>
    readdir(const boost::filesystem::path& p);

    void
    unlink(const boost::filesystem::path& p);

    void
    rename(const boost::filesystem::path& from,
           const boost::filesystem::path& to);

    void
    truncate(const boost::filesystem::path& p,
             uint64_t len);

    File
    open(const boost::filesystem::path& p,
         youtils::FDMode mode,
         CreateIfNecessary create_if_necessary = CreateIfNecessary::F);

    void
    chmod(const boost::filesystem::path& p,
          int mode);

    struct stat
    stat(const boost::filesystem::path& p);

    void
    utimes(const boost::filesystem::path& p, struct timeval *times);

    // integration with an event loop (poll):
    // while (...)
    // (1) get_fd() -> fd -- this needs to happen on each iteration as the fd could
    //                       change!
    // (2) which_events() -> evts
    // (3) poll(fd, evts) -> revts
    // (3) handle_events(revts)
    int
    get_fd();

    int
    which_events();

    void
    handle_events(int evts);

private:
    DECLARE_LOGGER("NfsMount");

    friend class File;

    std::shared_ptr<nfs_context> ctx_;
    const std::string server_;
    const boost::filesystem::path export_;

    template<typename... A>
    void
    convert_errors_(const char* desc,
                    int (*nfs_fun)(nfs_context*,
                                   A... args),
                    A... args);
};

}

#endif // !SIMPLE_NFS_MOUNT_H_
