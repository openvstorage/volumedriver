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

#include "BackendFactory.h"
#include "FileBackend.h"
#include "MemoryBackend.h"

namespace failovercache
{

namespace fs = boost::filesystem;
namespace vd = volumedriver;
namespace yt = youtils;

namespace
{

const std::string safetyfile(".failovercache");

const boost::optional<fs::path>&
maybe_make_dir(const boost::optional<fs::path>& p)
{
    if (p)
    {
        fs::create_directories(*p);
    }

    return p;
}

}

BackendFactory::BackendFactory(const boost::optional<fs::path>& path,
                               const boost::optional<size_t> file_backend_buffer_size)
    : root_(maybe_make_dir(path))
    , file_backend_buffer_size_(file_backend_buffer_size)
{
    if (root_)
    {
        const fs::path f(*root_ / safetyfile);

        if (not fs::exists(f) and
            not fs::is_empty(*root_))
        {
            LOG_ERROR("Cowardly refusing to use " << *root_ <<
                      " for failovercache because it is not empty");
            throw fungi::IOException("Not starting failover in nonempty dir");
        }

        lock_fd_ = std::make_unique<yt::FileDescriptor>(f,
                                                        yt::FDMode::Write,
                                                        CreateIfNecessary::T);

        file_lock_ = boost::unique_lock<yt::FileDescriptor>(*lock_fd_,
                                                            boost::try_to_lock);

        if (not file_lock_)
        {
            LOG_ERROR("Failed to lock " << lock_fd_->path());
            throw fungi::IOException("Not starting failovercache without lock");
        }

        for (fs::directory_iterator it(*root_); it != fs::directory_iterator(); ++it)
        {
            if (it->path() != lock_fd_->path())
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
}

BackendFactory::~BackendFactory()
{
    if (root_)
    {
        try
        {
            fs::directory_iterator end;
            for (fs::directory_iterator it(*root_); it != end; ++it)
            {
                LOG_INFO("Cleaning up " << it->path());
                fs::remove_all(it->path());
            }
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to clean up " << root_);
    }
}

std::unique_ptr<Backend>
BackendFactory::make_backend(const std::string& nspace,
                             const vd::ClusterSize csize)
{
    if (root_)
    {
        return std::make_unique<FileBackend>(*root_,
                                             nspace,
                                             csize,
                                             file_backend_buffer_size_);
    }
    else
    {
        return std::make_unique<MemoryBackend>(nspace,
                                               csize);
    }
}

}
