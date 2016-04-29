// Copyright 2016 iNuron NV
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

BackendFactory::BackendFactory(const boost::optional<fs::path>& path)
    : root_(maybe_make_dir(path))
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
                                             csize);
    }
    else
    {
        return std::make_unique<MemoryBackend>(nspace,
                                               csize);
    }
}

}
