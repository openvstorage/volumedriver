// Copyright 2015 Open vStorage NV
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
                                       const int32_t clustersize,
                                       unsigned timeout)
    : socket_(nullptr)
    , stream_(nullptr)
    , ns_(ns)
    , clustersize_(clustersize)
    , delete_failover_dir_(false)
{
    try
    {
        socket_ = fungi :: Socket::createClientSocket(cfg.host,
                                                      cfg.port);
        stream_ = new fungi :: IOBaseStream(socket_);
//        *stream_ << fungi::IOBaseStream::cork; --AT-- removed because this command is never sent over the wire
        *stream_ << fungi::IOBaseStream::RequestTimeout(timeout);
//        *stream_ << fungi::IOBaseStream::uncork;
        Register_();

    }
    catch(...)
    {
        if(stream_ != 0)
        {
            delete stream_;
        }

        if(socket_ != 0)
        {
            delete socket_;
        }

        throw;
    }
}

void
FailOverCacheProxy::getSCORange(SCO& oldest,
                                SCO& youngest)
{
    *stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(*stream_, GetSCORange);
    *stream_ << fungi::IOBaseStream::uncork;


    *stream_ >> fungi::IOBaseStream::cork;
    *stream_ >> oldest;
    *stream_ >> youngest;
}

void
FailOverCacheProxy::Register_()
{
    *stream_ << volumedriver::CommandData<Register>(ns_.str(),
                                                   clustersize_);
}

void
FailOverCacheProxy::checkStreamOK(const std::string& ex)
{
    *stream_ >> fungi::IOBaseStream::cork;
    uint32_t res;
    *stream_ >> res;
    if(res != volumedriver::Ok)
    {
        LOG_ERROR("FailOverCache Protocol Error, no Ok Returned in" << ex);
            throw fungi::IOException(ex.c_str());
    }
}

void
FailOverCacheProxy::Unregister_()
{
    if(delete_failover_dir_)
    {
        LOG_INFO("Deleting failover dir for " << ns_);
        *stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(*stream_, Unregister);
        *stream_ << fungi::IOBaseStream::uncork;
        return checkStreamOK(__FUNCTION__);
    }
    else
    {
        LOG_INFO("Not deleting failover dir for " << ns_);
    }


}

FailOverCacheProxy::~FailOverCacheProxy()
{
    try
    {
        Unregister_();
    }
    catch(std::exception& e)
    {
        LOG_ERROR("Could not cleanly close connection to the failover cache, will not be useable any more " << e.what());
    }

    catch(...)
    {
         LOG_ERROR("Could not cleanly close connection to the failover cache, will not be useable any more");
    }

    delete socket_;
    delete stream_;
}

void
FailOverCacheProxy::removeUpTo(const SCO sconame) throw ()
{
    try
    {
        *stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(*stream_,RemoveUpTo);
        *stream_ << sconame;
        *stream_ << fungi::IOBaseStream::uncork;
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
FailOverCacheProxy::setRequestTimeout(const uint32_t seconds)
{
    *stream_ << fungi::IOBaseStream::cork;
    *stream_ << fungi::IOBaseStream::RequestTimeout(seconds);
    *stream_ << fungi::IOBaseStream::uncork;
}

void
FailOverCacheProxy::addEntries( boost::ptr_vector<FailOverCacheEntry>& entries)
{
    const CommandData<AddEntries> comd(entries);
    *stream_ << comd;
}

void
FailOverCacheProxy::flush()
{
    *stream_ << CommandData<Flush>();
}

void
FailOverCacheProxy::clear()
{
    LOG_TRACE("Clearing failover cache");

    *stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(*stream_, Clear);
    *stream_ << fungi::IOBaseStream::uncork;
    checkStreamOK(__FUNCTION__);
}

}

// Local Variables: **
// mode: c++ **
// End: **
