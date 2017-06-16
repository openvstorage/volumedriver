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

#include "FailOverCacheAcceptor.h"
#include "FailOverCacheProtocol.h"
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
#include <youtils/Timer.h>

namespace volumedriver
{

namespace failovercache
{

namespace yt = youtils;

FailOverCacheProtocol::FailOverCacheProtocol(std::unique_ptr<fungi::Socket> sock,
                                             fungi::SocketServer& /*parentServer*/,
                                             FailOverCacheAcceptor& fact,
                                             const boost::chrono::microseconds busy_loop_duration)
    : sock_(std::move(sock))
    , stream_(*sock_)
    , fact_(fact)
    , use_rs_(sock_->isRdma())
    , stop_(false)
    , busy_loop_duration_(busy_loop_duration)
{
    sock_->setNonBlocking();
    if(pipe(pipes_) != 0)
    {
        stream_.close();
        throw fungi::IOException("could not not open pipe");
    }

    thread_ = new fungi::Thread(*this,
                                true);
};

FailOverCacheProtocol::~FailOverCacheProtocol()
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
    CATCH_STD_ALL_LOG_IGNORE("Problem shutting down the FailOverCacheProtocol");
}

void FailOverCacheProtocol::start()
{
    thread_->start();
}

void FailOverCacheProtocol::stop()
{
    stop_ = true;
    ssize_t ret;
    ret = write(pipes_[1],"a",1);
    if(ret < 0) {
    	LOG_ERROR("Failed to send stop request to thread: " << strerror(errno));
    }
}

bool
FailOverCacheProtocol::poll_(FailOverCacheCommand& cmd)
{
    struct pollfd fds[2];
    fds[0].fd = sock_->fileno();
    fds[1].fd = pipes_[0];

    fds[0].events = POLLIN | POLLPRI;
    fds[1].events = POLLIN | POLLPRI;
    fds[0].revents = POLLIN | POLLPRI;
    fds[1].revents = POLLIN | POLLPRI;

    int res = 0;

    yt::SteadyTimer t;

    while (not stop_)
    {
        if (t.elapsed() < busy_loop_duration_)
        {
            res = rs_recv(sock_->fileno(), &cmd, 0, 0);
            if (res < 0)
            {
                if (errno == EAGAIN or errno == EINTR)
                {
                    continue;
                }
                else
                {
                    LOG_ERROR("Error in rs_recv: " << strerror(errno));
                    throw fungi::IOException("Error in rs_recv");
                }
            }
            ASSERT(res == 0);
        }
        else
        {
            res = rs_poll(fds, 2, -1);
            ASSERT(res != 0);
            if (res < 0)
            {
                res = -errno;
                if (res != -EINTR)
                {
                    LOG_ERROR("Error in poll: " << strerror(-res));
                    throw fungi::IOException("Error in poll");
                }
            }
            else if (fds[1].revents or stop_)
            {
                break;
            }
        }

        stream_ >> fungi::IOBaseStream::cork;
        stream_ >> cmd;
        return true;
    }

    LOG_INFO("Stop requested");
    return false;
}

void
FailOverCacheProtocol::run()
{
    try {
        LOG_INFO("Connected");
        while (true)
        {
            FailOverCacheCommand com;

            try
            {
                if (not poll_(com))
                {
                    break;
                }
            }
            CATCH_STD_ALL_EWHAT({
                    LOG_INFO("Reading command from socked failed, will exit this thread: " << EWHAT);
                    break;
                });

            LOG_TRACE("Executing " << com);

            switch (com)
            {
#define CASE(x) case FailOverCacheCommand::x

            CASE(Register):
                register_();
                break;
            CASE(Unregister):
                unregister_();
                break;
            CASE(AddEntries):
                addEntries_();
                break;
            CASE(GetEntries):
                getEntries_();
                break;
            CASE(Flush):
                Flush_();
                break;
            CASE(Clear):
                Clear_();
                break;
            CASE(GetSCORange):
                getSCORange_();
                break;
            CASE(GetSCO):
                getSCO_();
                break;
            CASE(RemoveUpTo):
                removeUpTo_();
                break;
#undef CASE
            default:
                LOG_ERROR("DEFAULT BRANCH IN SWITCH...");
                throw fungi :: IOException("no valid command");
            }

            LOG_TRACE("Finished " << com);
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
FailOverCacheProtocol::register_()
{
    volumedriver::CommandData<FailOverCacheCommand::Register> data;
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
FailOverCacheProtocol::unregister_()
{
    VERIFY(cache_);
    LOG_INFO("Unregistering namespace " << cache_->getNamespace());
    try
    {
        // cache_->unregister_();
        fact_.remove(*cache_);
        cache_ = nullptr;
        returnOk();
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Error unregistering: " << EWHAT);
            returnNotOk();
        });
}

void
FailOverCacheProtocol::addEntries_()
{
    VERIFY(cache_);

    volumedriver::CommandData<FailOverCacheCommand::AddEntries> data;
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
FailOverCacheProtocol::returnOk()
{
    stream_ << fungi::IOBaseStream::cork;
    stream_ << FailOverCacheCommand::Ok;
    stream_ << fungi::IOBaseStream::uncork;
}

void
FailOverCacheProtocol::returnNotOk()
{
    stream_ << fungi::IOBaseStream::cork;
    stream_ << FailOverCacheCommand::NotOk;
    stream_ << fungi::IOBaseStream::uncork;
}

void
FailOverCacheProtocol::Flush_()
{
    VERIFY(cache_);
    LOG_TRACE("Flushing for namespace " <<  cache_->getNamespace());
    cache_->flush();
    returnOk();
}

void
FailOverCacheProtocol::Clear_()
{
    VERIFY(cache_);
    LOG_INFO("Clearing for namespace " << cache_->getNamespace());
    cache_->clear();
    returnOk();
}

void
FailOverCacheProtocol::processFailOverCacheEntry_(volumedriver::ClusterLocation cli,
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
FailOverCacheProtocol::removeUpTo_()
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
FailOverCacheProtocol::getEntries_()
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

    cache_->getEntries(boost::bind(&FailOverCacheProtocol::processFailOverCacheEntry_,
                                   this,
                                   _1,
                                   _2,
                                   _3,
                                   _4,
                                   true));
}

void
FailOverCacheProtocol::getSCO_()
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
                   boost::bind(&FailOverCacheProtocol::processFailOverCacheEntry_,
                               this,
                               _1,
                               _2,
                               _3,
                               _4,
                               false));
}

void
FailOverCacheProtocol::getSCORange_()
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

}

// Local Variables: **
// mode: c++ **
// End: **
