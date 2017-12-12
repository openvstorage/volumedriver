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

#include "FileBackend.h"
#include "FailOverCacheAcceptor.h"
#include "FailOverCacheProtocol.h"

#include <youtils/FileDescriptor.h>

namespace volumedriver
{

namespace failovercache
{

namespace fs = boost::filesystem;
namespace yt = youtils;

using namespace fungi;

#define LOCK()                                  \
    boost::lock_guard<decltype(mutex_)> lg_(mutex_)

FailOverCacheAcceptor::FailOverCacheAcceptor(const BackendFactory::Config& options,
                                             const boost::chrono::microseconds busy_loop_duration,
                                             const ProtocolFeatures features)
    : protocol_features(features)
    , factory_(options)
    , busy_loop_duration_(busy_loop_duration)
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
                                                  *this,
                                                  busy_loop_duration_));
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
FailOverCacheAcceptor::lookup(const std::string& nspace,
                              const ClusterSize csize,
                              const OwnerTag owner_tag)
{
    LOCK();

    BackendPtr w;

    auto it = map_.find(nspace);
    if (it != map_.end())
    {
        if (it->second->cluster_size() == csize)
        {
            w = it->second;
            w->setFirstCommandMustBeGetEntries();
        }
        else
        {
            LOG_ERROR(nspace << ": refusing registration with cluster size " <<
                      csize << " - we're already using " <<
                      it->second->cluster_size());
        }
    }
    else
    {
        w = factory_.make_backend(nspace,
                                  csize);

        auto res(map_.emplace(std::make_pair(nspace,
                                             w)));
        VERIFY(res.second);
    }

    if (w)
    {
        w->register_(owner_tag);
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

}

// Local Variables: **
// mode : c++ **
// End: **
