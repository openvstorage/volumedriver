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

#ifndef SIMPLE_NFS_FILE_H_
#define SIMPLE_NFS_FILE_H_

#include <memory>

#include <boost/filesystem.hpp>

#include <youtils/Logging.h>
#include <youtils/FileDescriptor.h>

struct nfs_context;
struct nfsfh;

namespace simple_nfs
{

class Mount;

class File
{
public:
    File(Mount& mnt,
         const boost::filesystem::path& p,
         youtils::FDMode mode,
         CreateIfNecessary create_if_necessary = CreateIfNecessary::F);

    ~File() = default;

    File(const File&) = default;

    File&
    operator=(const File&) = default;

    size_t
    pread(void* buf,
          size_t count,
          off_t off);

    using ReadCallback = std::function<void(ssize_t res)>;

    void
    pread(void* buf,
          size_t count,
          off_t off,
          ReadCallback rcb);

    size_t
    pwrite(const void* buf,
           size_t count,
           off_t off);

    using WriteCallback = std::function<void(ssize_t res)>;

    void
    pwrite(const void* buf,
           size_t count,
           off_t off,
           WriteCallback wcb);
    void
    fsync();

    void
    truncate(uint64_t len);

    struct stat
    stat();

private:
    DECLARE_LOGGER("SimpleNfsFile");

    std::shared_ptr<nfs_context> ctx_;
    std::shared_ptr<nfsfh> fh_;

    template<typename... A>
    int
    convert_errors_(const char* desc,
                    int (*nfs_fun)(nfs_context*, nfsfh*, A...),
                    A...);
};

}

#endif // !SIMPLE_NFS_FILE_H_
