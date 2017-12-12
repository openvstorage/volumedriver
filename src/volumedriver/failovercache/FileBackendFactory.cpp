// Copyright (C) 2017 iNuron NV
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

#include "FileBackendFactory.h"

#include <youtils/FileDescriptor.h>
#include <youtils/IOException.h>

namespace volumedriver
{

namespace failovercache
{

namespace fs = boost::filesystem;
namespace yt = youtils;

FileBackendFactory::FileBackendFactory(const FileBackend::Config& config)
    : config_(config)
{
    fs::create_directories(config_.path);
    const fs::path f(config_.path / ".failovercache");

        if (not fs::exists(f) and
            not fs::is_empty(config_.path))
        {
            LOG_ERROR("Cowardly refusing to use " << config_.path <<
                      " for failovercache because it is not empty");
            throw fungi::IOException("Not starting failovercache in nonempty dir");
        }

        fd_ = std::make_unique<yt::FileDescriptor>(f,
                                                   yt::FDMode::Write,
                                                   CreateIfNecessary::T);
        lock_ = boost::unique_lock<yt::FileDescriptor>(*fd_,
                                                       boost::try_to_lock);
        if (not lock_)
        {
            LOG_ERROR("Failed to lock " << fd_->path());
            throw fungi::IOException("Not starting failovercache without lock");
        }

        for (fs::directory_iterator it(config_.path); it != fs::directory_iterator(); ++it)
        {
            if (it->path() != fd_->path())
            {
                LOG_INFO("Removing " << it->path());
                fs::remove_all(it->path());
            }
            else
            {
                LOG_INFO("Not removing " << it->path());
            }
        }
}

FileBackendFactory::~FileBackendFactory()
{
    // `lock_` (and `fd_`) indicate whether this is the owner of the directory
    // or if ownership was moved - in the latter case we don't want to remove the
    // directory of course.
    if (lock_)
    {
        ASSERT(fd_);

        try
        {
            fs::directory_iterator end;
            for (fs::directory_iterator it(config_.path); it != end; ++it)
            {
                LOG_INFO("Cleaning up " << it->path());
                fs::remove_all(it->path());
            }
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to clean up " << config_.path);
    }
}

std::unique_ptr<FileBackend>
FileBackendFactory::make_one(const std::string& nspace,
                             const volumedriver::ClusterSize csize) const
{
    if (not lock_)
    {
        throw fungi::IOException("No exclusive ownership over directory");
    }

    ASSERT(fd_);

    return std::unique_ptr<FileBackend>(new FileBackend(config_,
                                                        nspace,
                                                        csize));
}

}

}
