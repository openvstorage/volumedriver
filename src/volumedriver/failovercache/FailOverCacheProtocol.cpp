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

#include "CapnProtoDispatcher.h"
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

#include <capnp/serialize.h>

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
    , owner_tag_(0)
    , fact_(fact)
    , use_rs_(sock_->isRdma())
    , stop_(false)
    , busy_loop_duration_(busy_loop_duration)
    , capnp_dispatcher_(std::make_unique<CapnProtoDispatcher>(*this))
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
            CASE(TunnelCapnProto):
                if ((fact_.protocol_features.t bitand ProtocolFeature::TunnelCapnProto))
                {
                    capnp_request_();
                    break;
                }
                else
                {
                    // fall through
                }
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

template<typename... Args>
void
FailOverCacheProtocol::handle_fungi_(void (FailOverCacheProtocol::*fn)(Args...),
                                     Args... args)
{
    try
    {
        (this->*fn)(args...);
        returnOk();
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(EWHAT);
            returnNotOk();
        });
}

void
FailOverCacheProtocol::do_register_(const std::string& nspace,
                                    const ClusterSize csize,
                                    const OwnerTag owner_tag)
{
    LOG_INFO("Registering namespace " << nspace <<
             ", cluster size " << csize << ", owner tag " << owner_tag);

    cache_ = fact_.lookup(nspace,
                          csize,
                          owner_tag);
    if (not cache_)
    {
        LOG_ERROR("Failed to register namespace " << nspace);
        throw fungi::IOException("Failed to register namespace",
                                 nspace.c_str());
    }

    owner_tag_ = owner_tag;

    VERIFY(cache_->owner_tag() == owner_tag_);
    VERIFY(cache_->registered());
}

void
FailOverCacheProtocol::register_()
{
    volumedriver::CommandData<FailOverCacheCommand::Register> cmd;
    stream_ >> cmd;

    handle_fungi_<const std::string&,
                  ClusterSize,
                  OwnerTag>(&FailOverCacheProtocol::do_register_,
                            cmd.ns_,
                            cmd.clustersize_,
                            // the old protocol doesn't know about
                            // OwnerTags but neither about qdepths > 1,
                            // so we use the reserved OwnerTag(0).
                            OwnerTag(0));
}

void
FailOverCacheProtocol::do_unregister_()
{
    VERIFY(cache_);
    LOG_INFO("Unregistering namespace " << cache_->getNamespace());
    fact_.remove(*cache_);
    cache_ = nullptr;
}

void
FailOverCacheProtocol::unregister_()
{
    handle_fungi_(&FailOverCacheProtocol::do_unregister_);
}

void
FailOverCacheProtocol::do_add_entries_(std::vector<FailOverCacheEntry> entries,
                                       std::unique_ptr<uint8_t[]> buf)
{
    // TODO: consider preventing empty AddEntries requests
    if (entries.empty())
    {
        VERIFY(buf == nullptr);
    }
    else
    {
        VERIFY(buf != nullptr);
        cache_->addEntries(std::move(entries),
                           std::move(buf),
                           owner_tag_);
    }
}

void
FailOverCacheProtocol::addEntries_()
{
    VERIFY(cache_);

    volumedriver::CommandData<FailOverCacheCommand::AddEntries> data;
    stream_ >> data;

    do_add_entries_(std::move(data.entries_),
                    std::move(data.buf_));
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
FailOverCacheProtocol::do_flush_()
{
    VERIFY(cache_);
    LOG_TRACE("Flushing for namespace " <<  cache_->getNamespace());
    cache_->flush(owner_tag_);
}

void
FailOverCacheProtocol::Flush_()
{
    do_flush_();
    returnOk();
}

void
FailOverCacheProtocol::do_clear_()
{
    VERIFY(cache_);
    LOG_INFO("Clearing for namespace " << cache_->getNamespace());
    cache_->clear(owner_tag_);
}

void
FailOverCacheProtocol::Clear_()
{
    do_clear_();
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
FailOverCacheProtocol::do_remove_up_to_(SCO sco)
{
    VERIFY(cache_);
    LOG_INFO("Namespace " << cache_->getNamespace() << ", SCO " << sco);
    cache_->removeUpTo(sco,
                       owner_tag_);
}

void
FailOverCacheProtocol::removeUpTo_()
{
    SCO sco;
    stream_ >> sco;
    handle_fungi_(&FailOverCacheProtocol::do_remove_up_to_,
                  sco);
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
    LOG_INFO("Namespace " << cache_->getNamespace());
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

std::pair<ClusterLocation, ClusterLocation>
FailOverCacheProtocol::do_get_range_()
{
    VERIFY(cache_);
    LOG_INFO("Namespace" << cache_->getNamespace());
    return cache_->range();
}

void
FailOverCacheProtocol::getSCORange_()
{
    ClusterLocation oldest;
    ClusterLocation youngest;
    std::tie(oldest, youngest) = do_get_range_();

    stream_ << fungi::IOBaseStream::cork;
    stream_ << oldest.sco();
    stream_ << youngest.sco();

    stream_ << fungi::IOBaseStream::uncork;
}

// fungi::Socket's corking does more than plain TCP_CORK - it also brings
// extra buffering into play and modifies the protocol. So open-code TCP
// corking here.
void
FailOverCacheProtocol::raw_cork_(bool set)
{
    int val = set ? 1 : 0;
    int ret = setsockopt(sock_->fileno(),
                         IPPROTO_TCP,
                         TCP_CORK,
                         &val,
                         sizeof(val));
    if (ret != 0)
    {
        LOG_ERROR("Failed to " << (set ? "cork" : "uncork") << ": " << strerror(errno));
        throw fungi::IOException(set ? "Failed to cork" : "Failed to uncork",
                                 "FailOverCacheProtocol",
                                 errno);
    }
}

void
FailOverCacheProtocol::capnp_request_()
{
    TunnelCapnProtoHeader hdr;
    CommandData<FailOverCacheCommand::TunnelCapnProto> cmd(hdr);
    stream_ >> cmd;

    VERIFY(hdr.capnp_size > 0);
    VERIFY(hdr.capnp_size % sizeof(capnp::word) == 0);

    auto capnp_buf(kj::heapArray<capnp::word>(hdr.capnp_size / sizeof(capnp::word)));

    {
        kj::ArrayPtr<capnp::byte> buf(capnp_buf.asPtr().asBytes());
        stream_.read(buf.begin(),
                     buf.size());
    }

    // A vector would be nicer as it also tracks the size - addEntries_ and its
    // callees need to be changed for that however!
    std::unique_ptr<uint8_t[]> bulk_data;
    if (hdr.data_size)
    {
        bulk_data = std::make_unique<uint8_t[]>(hdr.data_size);
        stream_.read(bulk_data.get(),
                     hdr.data_size);

    }

    // TODO: push any knowledge of capnp out, into the CapnProtoDispatcher.
    capnp::MallocMessageBuilder mb;
    std::vector<uint8_t> rsp_data((*capnp_dispatcher_)(mb,
                                                       std::move(capnp_buf),
                                                       std::move(bulk_data),
                                                       hdr.data_size));

    kj::Array<capnp::word> rsp_capnp_array(capnp::messageToFlatArray(mb));
    kj::ArrayPtr<capnp::byte> rsp_capnp(rsp_capnp_array.asBytes());
    hdr.capnp_size = rsp_capnp.size();
    hdr.data_size = rsp_data.size();

    VERIFY(hdr.capnp_size);

    raw_cork_(true);
    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         raw_cork_(false);
                                     }));

    stream_.write(reinterpret_cast<const uint8_t*>(&hdr),
                  sizeof(hdr));
    stream_.write(rsp_capnp.begin(),
                  rsp_capnp.size());

    if (hdr.data_size)
    {
        stream_.write(rsp_data.data(),
                      rsp_data.size());
    }
}

}

}

// Local Variables: **
// mode: c++ **
// End: **
