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

#include "FailOverCacheAcceptor.h"
#include "FailOverCacheProtocol.h"

#include <youtils/FileDescriptor.h>

namespace failovercache
{

namespace fs = boost::filesystem;
namespace yt = youtils;

using namespace volumedriver;
using namespace fungi;

static const std::string safetyfile("/.failovercache");

namespace
{

const fs::path&
maybe_make_dir(const fs::path& p)
{
    fs::create_directories(p);
    return p;
}

}

FailOverCacheAcceptor::FailOverCacheAcceptor(const fs::path& path)
    : volCacheMap_(path)
    , m("FailOverCacheAcceptor")
    , root_(maybe_make_dir(path))
{
    const fs::path f(root_ / safetyfile);

    if (not fs::exists(f) and
        not fs::is_empty(root_))
    {
        LOG_ERROR("Cowardly refusing to use " << root_ <<
                  " for failovercache because it is not empty");
        throw fungi::IOException("Not starting failover in nonempty dir");
    }

    lock_fd_ = std::make_unique<yt::FileDescriptor>(f,
                                                    yt::FDMode::Write,
                                                    CreateIfNecessary::T);

    file_lock_ = std::unique_lock<yt::FileDescriptor>(*lock_fd_,
                                                      std::try_to_lock);

    if (not file_lock_)
    {
        LOG_ERROR("Failed to lock " << lock_fd_->path());
        throw fungi::IOException("Not starting failovercache without lock");
    }

    for (fs::directory_iterator it(root_); it != fs::directory_iterator(); ++it)
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

FailOverCacheAcceptor::~FailOverCacheAcceptor()
{
    int count = 0;
    {
        ScopedLock l(m);

        for(iter i = protocols.begin();
            i != protocols.end();
            ++i)
        {
            (*i)->stop();
        }
        count = protocols.size();
    }
    while (count > 0)
    {
        // Wait for the threads to clean up the protocols
        sleep(1);
        {
            ScopedLock l(m);
            count = protocols.size();
        }
    }
}

Protocol*
FailOverCacheAcceptor::createProtocol(fungi::Socket *s,
                                      fungi::SocketServer& parentServer)
{
    ScopedLock l(m);
    protocols.push_back(new FailOverCacheProtocol(s, parentServer,*this));
    return protocols.back();
}
}

// Local Variables: **
// mode : c++ **
// End: **
