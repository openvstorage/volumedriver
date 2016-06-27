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
#include "DtlProxy.h"
#include "Volume.h"

#include <youtils/IOException.h>

namespace volumedriver
{

using namespace fungi;

DtlProxy::DtlProxy(const FailOverCacheConfig& cfg,
                                       const Namespace& ns,
                                       const LBASize lba_size,
                                       const ClusterMultiplier cluster_mult,
                                       const boost::chrono::seconds timeout)
    : socket_(fungi::Socket::createClientSocket(cfg.host,
                                                cfg.port))
    , stream_(*socket_)
    , ns_(ns)
    , lba_size_(lba_size)
    , cluster_mult_(cluster_mult)
    , delete_failover_dir_(false)
{
    //        stream_ << fungi::IOBaseStream::cork; --AT-- removed because this command is never sent over the wire
    stream_ << fungi::IOBaseStream::RequestTimeout(timeout.count());
    //        stream_ << fungi::IOBaseStream::uncork;
    register_();
}

DtlProxy::~DtlProxy()
{
    try
    {
        unregister_();
    }
    CATCH_STD_ALL_LOG_IGNORE("Could not cleanly close connection to the failover cache, will not be useable any more");
}

void
DtlProxy::getSCORange(SCO& oldest,
                                SCO& youngest)
{
    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, GetSCORange);
    stream_ << fungi::IOBaseStream::uncork;
    stream_ >> fungi::IOBaseStream::cork;
    stream_ >> oldest;
    stream_ >> youngest;
}

void
DtlProxy::register_()
{
    const ClusterSize csize(static_cast<size_t>(cluster_mult_) *
                            static_cast<size_t>(lba_size_));
    stream_ << volumedriver::CommandData<Register>(ns_.str(),
                                                    csize);
}

void
DtlProxy::checkStreamOK(const std::string& ex)
{
    stream_ >> fungi::IOBaseStream::cork;
    uint32_t res;
    stream_ >> res;
    if(res != volumedriver::Ok)
    {
        LOG_ERROR("FailOverCache Protocol Error, no Ok Returned in" << ex);
            throw fungi::IOException(ex.c_str());
    }
}

void
DtlProxy::unregister_()
{
    if(delete_failover_dir_)
    {
        LOG_INFO("Deleting failover dir for " << ns_);
        stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(stream_, Unregister);
        stream_ << fungi::IOBaseStream::uncork;
        return checkStreamOK(__FUNCTION__);
    }
    else
    {
        LOG_INFO("Not deleting failover dir for " << ns_);
    }
}

void
DtlProxy::removeUpTo(const SCO sconame) throw ()
{
    try
    {
        stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(stream_,RemoveUpTo);
        stream_ << sconame;
        stream_ << fungi::IOBaseStream::uncork;
        return checkStreamOK(__FUNCTION__);
    }
    catch(std::exception& e)
    {
        LOG_ERROR("Exception caught: " << e.what());
    }
    catch(...)
    {
        LOG_ERROR("Unknown exception caught");
    }
}

void
DtlProxy::setRequestTimeout(const boost::chrono::seconds seconds)
{
    stream_ << fungi::IOBaseStream::cork;
    stream_ << fungi::IOBaseStream::RequestTimeout(seconds.count());
    stream_ << fungi::IOBaseStream::uncork;
}

void
DtlProxy::addEntries(std::vector<DtlEntry> entries)
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

    const CommandData<AddEntries> comd(std::move(entries));
    stream_ << comd;
}

void
DtlProxy::flush()
{
    stream_ << CommandData<Flush>();
}

void
DtlProxy::clear()
{
    LOG_TRACE("Clearing failover cache");

    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, Clear);
    stream_ << fungi::IOBaseStream::uncork;
    checkStreamOK(__FUNCTION__);
}

void
DtlProxy::getEntries(SCOProcessorFun processor)
{
    // Y42 review later
    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, GetEntries);
    stream_ << fungi::IOBaseStream::uncork;

    getObject_(processor, true);
}

uint64_t
DtlProxy::getSCOFromFailOver(SCO a,
                                       SCOProcessorFun processor)
{
    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, GetSCO);
    stream_ << a;
    stream_ << fungi::IOBaseStream::uncork;
    return getObject_(processor, false);
}

uint64_t
DtlProxy::getObject_(SCOProcessorFun processor,
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
            processor(cli, lba, buf.data(), size);
            ret += size;
        }
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
