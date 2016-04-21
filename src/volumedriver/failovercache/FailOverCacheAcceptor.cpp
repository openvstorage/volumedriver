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

#include "FileBackend.h"
#include "FailOverCacheAcceptor.h"
#include "FailOverCacheProtocol.h"

#include <youtils/FileDescriptor.h>

namespace failovercache
{

namespace fs = boost::filesystem;
namespace yt = youtils;

using namespace volumedriver;
using namespace fungi;

#define LOCK()                                  \
    boost::lock_guard<decltype(mutex_)> lg_(mutex_)

FailOverCacheAcceptor::FailOverCacheAcceptor(const boost::optional<fs::path>& path)
    : factory_(path)
{}

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
FailOverCacheAcceptor::createProtocol(std::unique_ptr<fungi::Socket> s,
                                      fungi::SocketServer& parentServer)
{
    LOCK();
    protocols.push_back(new FailOverCacheProtocol(std::move(s),
                                                  parentServer,
                                                  *this));
    return protocols.back();
}

void
FailOverCacheAcceptor::remove(Backend& w)
{
    LOCK();

    const auto res(map_.erase(w.getNamespace()));
    if (res != 1)
    {
        LOG_WARN("Request to remove namespace " << w.getNamespace() <<
                 " which is not registered here");
    }
}

FailOverCacheAcceptor::BackendPtr
FailOverCacheAcceptor::lookup(const CommandData<Register>& reg)
{
    LOCK();

    BackendPtr w;

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
        w = factory_.make_backend(reg.ns_,
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

FailOverCacheAcceptor::BackendPtr
FailOverCacheAcceptor::find_backend_(const std::string& ns)
{
    LOCK();

    auto it = map_.find(ns);
    if (it != map_.end())
    {
        return it->second;
    }
    else
    {
        return nullptr;
    }
}

}

// Local Variables: **
// mode : c++ **
// End: **
