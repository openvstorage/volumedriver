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

#include "FailOverCacheConfig.h"
#include "FailOverCacheProxy.h"
#include "Volume.h"

#include <youtils/IOException.h>

namespace volumedriver
{

using namespace fungi;

FailOverCacheProxy::FailOverCacheProxy(const FailOverCacheConfig& cfg,
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

FailOverCacheProxy::~FailOverCacheProxy()
{
    try
    {
        unregister_();
    }
    CATCH_STD_ALL_LOG_IGNORE("Could not cleanly close connection to the failover cache, will not be useable any more");
}

void
FailOverCacheProxy::getSCORange(SCO& oldest,
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
FailOverCacheProxy::register_()
{
    const ClusterSize csize(static_cast<size_t>(cluster_mult_) *
                            static_cast<size_t>(lba_size_));
    stream_ << volumedriver::CommandData<Register>(ns_.str(),
                                                    csize);
}

void
FailOverCacheProxy::checkStreamOK(const std::string& ex)
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
FailOverCacheProxy::unregister_()
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
FailOverCacheProxy::removeUpTo(const SCO sconame) throw ()
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
FailOverCacheProxy::setRequestTimeout(const boost::chrono::seconds seconds)
{
    stream_ << fungi::IOBaseStream::cork;
    stream_ << fungi::IOBaseStream::RequestTimeout(seconds.count());
    stream_ << fungi::IOBaseStream::uncork;
}

void
FailOverCacheProxy::addEntries(std::vector<FailOverCacheEntry> entries)
{
    const CommandData<AddEntries> comd(std::move(entries));
    stream_ << comd;
}

void
FailOverCacheProxy::flush()
{
    stream_ << CommandData<Flush>();
}

void
FailOverCacheProxy::clear()
{
    LOG_TRACE("Clearing failover cache");

    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, Clear);
    stream_ << fungi::IOBaseStream::uncork;
    checkStreamOK(__FUNCTION__);
}

void
FailOverCacheProxy::getEntries(SCOProcessorFun processor)
{
    // Y42 review later
    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, GetEntries);
    stream_ << fungi::IOBaseStream::uncork;

    getObject_(processor);
}

uint64_t
FailOverCacheProxy::getSCOFromFailOver(SCO a,
                                       SCOProcessorFun processor)
{
    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, GetSCO);
    stream_ << a;
    stream_ << fungi::IOBaseStream::uncork;
    return getObject_(processor);
}

uint64_t
FailOverCacheProxy::getObject_(SCOProcessorFun processor)
{
    fungi::Buffer buf;
    stream_ >> fungi::IOBaseStream::cork;
    uint64_t ret = 0;
    while (true)
    {
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
