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

#include "Acceptor.h"
#include "DtlProtocol.h"
#include "FailOverCacheStreamers.h"
#include "fungilib/WrapByteArray.h"
#include "fungilib/use_rs.h"

#include <signal.h>
#include <unistd.h>
#include <poll.h>

#include <cerrno>
#include <cstring>

#include <rdma/rdma_cma.h>
#include <rdma/rsocket.h>

#include <youtils/Assert.h>
#include <youtils/ScopeExit.h>

namespace distributed_transaction_log
{

namespace yt = youtils;
using namespace volumedriver;

DtlProtocol::DtlProtocol(std::unique_ptr<fungi::Socket> sock,
                                             fungi::SocketServer& /*parentServer*/,
                                             Acceptor& fact)
    : sock_(std::move(sock))
    , stream_(*sock_)
    , fact_(fact)
    , use_rs_(sock_->isRdma())
{
    if(pipe(pipes_) != 0)
    {
        stream_.close();
        throw fungi::IOException("could not not open pipe");
    }
    nfds_ = std::max(sock_->fileno(), pipes_[0]) + 1;
    thread_ = new fungi::Thread(*this,
                                true);
};

DtlProtocol::~DtlProtocol()
{
    fact_.removeProtocol(this);
    close(pipes_[0]);
    close(pipes_[1]);

    // if(cache_)
    // {
    //     /cache_->unregister_();
    // }

    try
    {
        stream_.close();
        thread_->destroy(); // Yuck: this call does a "delete this" ...
    }
    CATCH_STD_ALL_LOG_IGNORE("Problem shutting down the DtlProtocol");
}

void DtlProtocol::start()
{
    thread_->start();
}

void DtlProtocol::stop()
{
    ssize_t ret;
    ret = write(pipes_[1],"a",1);
    if(ret < 0) {
    	LOG_ERROR("Failed to send stop request to thread: " << strerror(errno));
    }
}

void
DtlProtocol::run()
{
    try {
        LOG_INFO("Connected");
        while (true)
        {
            int32_t com = 0;

            try
            {
                struct pollfd fds[2];
                fds[0].fd = sock_->fileno();
                fds[1].fd = pipes_[0];

                fds[0].events = POLLIN | POLLPRI;
                fds[1].events = POLLIN | POLLPRI;
                fds[0].revents = POLLIN | POLLPRI;
                fds[1].revents = POLLIN | POLLPRI;

                int res = rs_poll(fds, 2, -1);

                if(res < 0)
                {
                    if(errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        LOG_ERROR("Error in select");
                        throw fungi::IOException("Error in select");
                    }
                }
                else if(fds[1].revents)
                {
                    LOG_INFO("Stop Requested, breaking out main loop");
                    break;
                }
                else
                {
                    stream_ >> fungi::IOBaseStream::cork;
                    stream_ >> com;
                }

            }
            CATCH_STD_ALL_EWHAT({
                    LOG_INFO("Reading command from socked failed, will exit this thread: " << EWHAT);
                    break;
                });

            switch (com)
            {

            case volumedriver::Register:
                LOG_TRACE("Executing Register");
                register_();
                LOG_TRACE("Finished Register");
                break;

            case volumedriver::Unregister:
                LOG_TRACE("Executing Unregister");
                unregister_();
                LOG_TRACE("Finished Unregister");
                break;

            case volumedriver::AddEntries:
                LOG_TRACE("Executing AddEntries");
                addEntries_();
                LOG_TRACE("Finished AddEntries");
                break;
            case volumedriver::GetEntries:
                LOG_TRACE("Executing GetEntries");
                getEntries_();
                LOG_TRACE("Finished GetEntries");
                break;
            case volumedriver::Flush:
                LOG_TRACE("Executing Flush");
                Flush_();
                LOG_TRACE("Finished Flush");
                break;

            case volumedriver::Clear:
                LOG_TRACE("Executing Clear");
                Clear_();
                LOG_TRACE("Finished Clear");
                break;

            case volumedriver::GetSCORange:
                LOG_TRACE("Executing GetSCORange");
                getSCORange_();
                LOG_TRACE("Finished GetSCORange");
                break;

            case volumedriver::GetSCO:
                LOG_TRACE("Executing GetSCO");
                getSCO_();
                LOG_TRACE("Finished GetSCO");
                break;
            case volumedriver::RemoveUpTo:
                LOG_TRACE("Executing RemoveUpTo");
                removeUpTo_();
                LOG_TRACE("Finished RemoveUpTo");

                break;
            default:
                LOG_ERROR("DEFAULT BRANCH IN SWITCH...");
                throw fungi :: IOException("no valid command");
            }
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Exception in thread: " << EWHAT);
            returnNotOk();
        });

    if(cache_)
    {
        LOG_INFO("Exiting cache server for namespace: " << cache_->getNamespace());
    }
    else
    {
        LOG_INFO("Exiting cache server before even registering (cache_ == 0), probably framework ping");
    }
}

void
DtlProtocol::register_()
{
    volumedriver::CommandData<volumedriver::Register> data;
    stream_ >> data;

    LOG_INFO("Registering namespace " << data.ns_);
    cache_ = fact_.lookup(data);
    if(not cache_)
    {
        returnNotOk();
    }
    else
    {
        VERIFY(cache_->registered());
        returnOk();
    }
}

void
DtlProtocol::unregister_()
{
    VERIFY(cache_);
    LOG_INFO("Unregistering namespace " << cache_->getNamespace());
    try
    {
        // cache_->unregister_();
        fact_.remove(*cache_);
        cache_ = nullptr;
        stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(stream_,volumedriver::Ok);
        stream_ << fungi::IOBaseStream::uncork;
    }
    catch(...)
    {
        stream_ << fungi::IOBaseStream::cork;
        OUT_ENUM(stream_, volumedriver::NotOk);
        stream_ << fungi::IOBaseStream::uncork;
    }

}

void
DtlProtocol::addEntries_()
{
    VERIFY(cache_);

    volumedriver::CommandData<volumedriver::AddEntries> data;
    stream_ >> data;

    // TODO: consider preventing empty AddEntries requests
    if (data.entries_.empty())
    {
        VERIFY(data.buf_ == nullptr);
    }
    else
    {
        VERIFY(data.buf_ != nullptr);
        cache_->addEntries(std::move(data.entries_),
                           std::move(data.buf_));
    }

    returnOk();
}

void
DtlProtocol::returnOk()
{
    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, volumedriver::Ok);
    stream_ << fungi::IOBaseStream::uncork;
}

void
DtlProtocol::returnNotOk()
{
    stream_ << fungi::IOBaseStream::cork;
    OUT_ENUM(stream_, volumedriver::NotOk);
    stream_ << fungi::IOBaseStream::uncork;
}

void
DtlProtocol::Flush_()
{
    VERIFY(cache_);
    LOG_TRACE("Flushing for namespace " <<  cache_->getNamespace());
    cache_->flush();
    returnOk();
}

void
DtlProtocol::Clear_()
{
    VERIFY(cache_);
    LOG_INFO("Clearing for namespace " << cache_->getNamespace());
    cache_->clear();
    returnOk();
}

void
DtlProtocol::processFailOverCacheEntry_(volumedriver::ClusterLocation cli,
                                                  int64_t lba,
                                                  const byte* buf,
                                                  int64_t size,
                                                  bool cork)
{
    LOG_TRACE("Sending Entry for lba " << lba);
    if (cork)
    {
        stream_ << fungi::IOBaseStream::cork;
    }
    stream_ << cli;
    stream_ << lba;
    const fungi::WrapByteArray a((byte*)buf, (int32_t)size);
    stream_ << a;
    if (cork)
    {
        stream_ << fungi::IOBaseStream::uncork;
    }
}

void
DtlProtocol::removeUpTo_()
{
    VERIFY(cache_);
    LOG_INFO("Namespace " << cache_->getNamespace());


    volumedriver::SCO sconame;

    stream_ >> sconame;
    try
    {
        cache_->removeUpTo(sconame);
        returnOk();
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(cache_->getNamespace() <<
                      ": caught exception removing SCOs up to " <<
                      sconame << ": " << EWHAT);
            returnNotOk();
        });
}

void
DtlProtocol::getEntries_()
{
    VERIFY(cache_);
    LOG_INFO("Namespace " <<  cache_->getNamespace());

    auto on_exit(yt::make_scope_exit([&]
    {
        try
        {
            stream_ << fungi::IOBaseStream::cork;
            volumedriver::ClusterLocation end_cli;
            stream_ << end_cli;
            stream_ << fungi::IOBaseStream::uncork;
        }
        CATCH_STD_ALL_LOGLEVEL_IGNORE("Could not send eof data to FOC at the other end",
                                      WARN);
    }));

    cache_->getEntries(boost::bind(&DtlProtocol::processFailOverCacheEntry_,
                                   this,
                                   _1,
                                   _2,
                                   _3,
                                   _4,
                                   true));
}

void
DtlProtocol::getSCO_()
{
    VERIFY(cache_);
    LOG_INFO("Namespace" << cache_->getNamespace());
    SCO scoName;

    stream_ >> scoName;

    auto on_exit(yt::make_scope_exit([&]
    {
        try
        {
            volumedriver::ClusterLocation end_cli;
            stream_ << end_cli;
            stream_ << fungi::IOBaseStream::uncork;
        }
        CATCH_STD_ALL_LOGLEVEL_IGNORE("Could not send eof data to FOC at the other end",
                                      WARN);
    }));

    stream_ << fungi::IOBaseStream::cork;
    cache_->getSCO(scoName,
                   boost::bind(&DtlProtocol::processFailOverCacheEntry_,
                               this,
                               _1,
                               _2,
                               _3,
                               _4,
                               false));
}

void
DtlProtocol::getSCORange_()
{
    VERIFY(cache_);
    LOG_INFO("Namespace" << cache_->getNamespace());
    SCO oldest;
    SCO youngest;
    cache_->getSCORange(oldest,
                        youngest);

    stream_ << fungi::IOBaseStream::cork;
    stream_ << oldest;
    stream_ << youngest;

    stream_ << fungi::IOBaseStream::uncork;
}

}

// Local Variables: **
// mode: c++ **
// End: **
