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

#include "FailOverCacheConfig.h"
#include "FailOverCacheProxy.h"
#include "FailOverCacheStreamers.h"
#include "Volume.h"

#include <youtils/IOException.h>

namespace volumedriver
{

using namespace fungi;

namespace bc = boost::chrono;

FailOverCacheProxy::FailOverCacheProxy(const FailOverCacheConfig& cfg,
                                       const Namespace& ns,
                                       const LBASize lba_size,
                                       const ClusterMultiplier cluster_mult,
                                       const boost::optional<bc::milliseconds>& request_timeout,
                                       const boost::optional<bc::milliseconds>& connect_timeout)
    : failovercache::ClientInterface(cfg,
                                     ns,
                                     lba_size,
                                     cluster_mult,
                                     request_timeout,
                                     connect_timeout)
    , socket_(fungi::Socket::createClientSocket(cfg.host,
                                                cfg.port,
                                                connect_timeout))
    , stream_(*socket_)
{
    socket_->setNonBlocking();
    setRequestTimeout(request_timeout);
    register_();
}

FailOverCacheProxy::~FailOverCacheProxy()
{
    try
    {
        unregister_();
    }
    CATCH_STD_ALL_LOG_IGNORE("Could not cleanly close connection to the failover cache, will not be useable any more");
}

template<FailOverCacheCommand cmd>
void
FailOverCacheProxy::send_()
{
    stream_ << fungi::IOBaseStream::cork;
    stream_ << cmd;
    stream_ << fungi::IOBaseStream::uncork;
}

template<FailOverCacheCommand cmd,
         typename... Args>
void
FailOverCacheProxy::send_(Args&&... args)
{
    stream_ << fungi::IOBaseStream::cork;
    stream_ << cmd;
    stream_ << CommandData<cmd>(std::forward<Args>(args)...);
    stream_ << fungi::IOBaseStream::uncork;
}

void
FailOverCacheProxy::check_(const char* desc)
{
    stream_ >> fungi::IOBaseStream::cork;
    FailOverCacheCommand res;
    stream_ >> res;
    if(res != FailOverCacheCommand::Ok)
    {
        LOG_ERROR("DTL sent status " << res << " != OK in response to " << desc);
        throw fungi::IOException("DTL sent status != OK",
                                 desc);
    }
}

void
FailOverCacheProxy::getSCORange(SCO& oldest,
                                SCO& youngest)
{
    send_<FailOverCacheCommand::GetSCORange>();
    stream_ >> fungi::IOBaseStream::cork;
    stream_ >> oldest;
    stream_ >> youngest;
}

void
FailOverCacheProxy::register_()
{
    const ClusterSize csize(static_cast<size_t>(cluster_mult_) *
                            static_cast<size_t>(lba_size_));
    send_<FailOverCacheCommand::Register>(nspace_.str(),
                                          csize);
    check_(__FUNCTION__);
}

void
FailOverCacheProxy::unregister_()
{
    if(delete_failover_dir_)
    {
        LOG_INFO(nspace_ << ": deleting failover data");
        send_<FailOverCacheCommand::Unregister>();
        check_(__FUNCTION__);
    }
    else
    {
        LOG_INFO(nspace_ << ": not deleting failover data");
    }
}

void
FailOverCacheProxy::removeUpTo(const SCO sconame) noexcept
{
    try
    {
        send_<FailOverCacheCommand::RemoveUpTo>(sconame);
        check_(__FUNCTION__);
    }
    CATCH_STD_ALL_LOG_IGNORE(nspace_ << ": failed to remove SCOs up to " << sconame);
}

void
FailOverCacheProxy::setRequestTimeout(const failovercache::ClientInterface::MaybeMilliSeconds& t)
{
    stream_ << fungi::IOBaseStream::RequestTimeout(t ? t->count() / 1000 : 0);
    failovercache::ClientInterface::setRequestTimeout(t);
}

void
FailOverCacheProxy::setBusyLoopDuration(const bc::microseconds& usecs)
{
    socket_->setBusyLoopDuration(usecs);
}

boost::future<void>
FailOverCacheProxy::addEntries(std::vector<FailOverCacheEntry> entries)
{
#ifndef NDEBUG
    if (not entries.empty())
    {
        const SCO sco = entries.front().cli_.sco();
        for (const auto& e : entries)
        {
            ASSERT(e.cli_.sco() == sco);
        }
    }
#endif

    send_<FailOverCacheCommand::AddEntries>(std::move(entries));
    check_(__FUNCTION__);

    return boost::make_ready_future();
}

boost::future<void>
FailOverCacheProxy::addEntries(const std::vector<ClusterLocation>& locs,
                               uint64_t start_address,
                               const uint8_t* data)
{
    const size_t cluster_size =
        static_cast<size_t>(lba_size()) *
        static_cast<size_t>(cluster_multiplier());

    std::vector<FailOverCacheEntry> entries;
    entries.reserve(locs.size());
    uint64_t lba = start_address;
    for (size_t i = 0; i < locs.size(); i++)
    {
        entries.emplace_back(locs[i],
                             lba, data + i * cluster_size,
                             cluster_size);

        lba += cluster_multiplier();
    }

    return addEntries(std::move(entries));
}

boost::future<void>
FailOverCacheProxy::flush()
{
    send_<FailOverCacheCommand::Flush>();
    check_(__FUNCTION__);
    return boost::make_ready_future();
}

void
FailOverCacheProxy::clear()
{
    send_<FailOverCacheCommand::Clear>();
    check_(__FUNCTION__);
}

void
FailOverCacheProxy::getEntries(SCOProcessorFun processor)
{
    // Y42 review later
    send_<FailOverCacheCommand::GetEntries>();
    getObject_(processor, true);
}

uint64_t
FailOverCacheProxy::getSCOFromFailOver(SCO sco,
                                       SCOProcessorFun processor)
{
    send_<FailOverCacheCommand::GetSCO>(sco);
    return getObject_(processor, false);
}

uint64_t
FailOverCacheProxy::getObject_(SCOProcessorFun processor,
                               bool cork_per_cluster)
{
    fungi::Buffer buf;
    uint64_t ret = 0;

    if (not cork_per_cluster)
    {
        stream_ >> fungi::IOBaseStream::cork;
    }

    while (true)
    {
        if (cork_per_cluster)
        {
            stream_ >> fungi::IOBaseStream::cork;
        }

        ClusterLocation cli;
        stream_ >> cli;
        if(cli.isNull())
        {
            return ret;
        }
        else
        {
            uint64_t lba = 0;
            stream_ >> lba;

            int64_t bal; // byte array length
            stream_ >> bal;

            int32_t size = (int32_t) bal;
            buf.store(stream_.getSink(), size);
            processor(cli, Lba(lba), buf.data(), size);
            ret += size;
        }
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
