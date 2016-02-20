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

#define LOCK()                                  \
    boost::lock_guard<decltype(mutex_)> lg_(mutex_)

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
    : root_(maybe_make_dir(path))
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

    file_lock_ = boost::unique_lock<yt::FileDescriptor>(*lock_fd_,
                                                        boost::try_to_lock);

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
        LOCK();

        for (auto& p : protocols)
        {
            p->stop();
        }
        count = protocols.size();
    }
    while (count > 0)
    {
        // Wait for the threads to clean up the protocols
        sleep(1);
        {
            LOCK();
            count = protocols.size();
        }
    }
}

Protocol*
FailOverCacheAcceptor::createProtocol(fungi::Socket *s,
                                      fungi::SocketServer& parentServer)
{
    LOCK();
    protocols.push_back(new FailOverCacheProtocol(s, parentServer,*this));
    return protocols.back();
}

void
FailOverCacheAcceptor::remove(FailOverCacheWriter& w)
{
    LOCK();

    const auto res(map_.erase(w.getNamespace()));
    if (res != 1)
    {
        LOG_WARN("Request to remove namespace " << w.getNamespace() <<
                 " which is not registered here");
    }
}

std::shared_ptr<FailOverCacheWriter>
FailOverCacheAcceptor::lookup(const CommandData<Register>& reg)
{
    LOCK();

    std::shared_ptr<FailOverCacheWriter> w;

    auto it = map_.find(reg.ns_);
    if (it != map_.end())
    {
        if (it->second->cluster_size() == reg.clustersize_)
        {
            w = it->second;
            w->setFirstCommandMustBeGetEntries();
        }
        else
        {
            LOG_ERROR(reg.ns_ << ": refusing registration with cluster size " <<
                      reg.clustersize_ << " - we're already using " <<
                      it->second->cluster_size());
        }
    }
    else
    {
        w = std::make_shared<FailOverCacheWriter>(root_,
                                                  reg.ns_,
                                                  reg.clustersize_);

        auto res(map_.emplace(std::make_pair(reg.ns_,
                                             w)));
        VERIFY(res.second);
    }

    if (w)
    {
        w->register_();
    }

    return w;
}
}

// Local Variables: **
// mode : c++ **
// End: **
