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

#include "ZUtils.h"
#include "ZWorkerPool.h"

#include <sys/eventfd.h>

#include <boost/lexical_cast.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>

namespace volumedriverfs
{

namespace yt = youtils;

namespace
{

DECLARE_LOGGER("ZWorkerPoolHelpers");

// push these two to ZUtils?
ZWorkerPool::MessageParts
recv_parts(zmq::socket_t& zock)
{
    LOG_TRACE("receiving message parts");

    ZWorkerPool::MessageParts parts;

    do
    {
        if (parts.size() == parts.capacity())
        {
            parts.reserve(parts.size() + 8);
        }

        zmq::message_t part;
        zock.recv(&part);

        LOG_TRACE("got message part");

        parts.emplace_back(std::move(part));
    }
    while (ZUtils::more_message_parts(zock));

    LOG_TRACE("got " << parts.size() << " message parts");

    return parts;
}

void
send_parts(zmq::socket_t& zock, ZWorkerPool::MessageParts& parts)
{
    LOG_TRACE("sending " << parts.size() << " message parts");

    for (size_t i = 0; i < parts.size(); ++i)
    {
        zock.send(parts[i],
                  (i + 1 == parts.size()) ?
                  0 :
                  ZMQ_SNDMORE);
    }
}

}

#define LOCK_TX()                                               \
    boost::lock_guard<decltype(tx_mutex_)> lgtx__(tx_mutex_)

#define LOCK_RX()                                               \
    boost::lock_guard<decltype(rx_mutex_)> lgrx__(rx_mutex_)

ZWorkerPool::ZWorkerPool(const std::string& name,
                         zmq::context_t& ztx,
                         const yt::Uri& uri,
                         uint16_t num_workers,
                         WorkerFun worker_fun,
                         DispatchFun dispatch_fun)
    : worker_fun_(std::move(worker_fun))
    , dispatch_fun_(std::move(dispatch_fun))
    , ztx_(ztx)
    , name_(name)
    , uri_(uri)
    , stop_(false)
{
    THROW_UNLESS(num_workers > 0);

    zmq::socket_t zock(ztx_, ZMQ_ROUTER);
    ZUtils::socket_no_linger(zock);

    const auto addr(boost::lexical_cast<std::string>(uri_));
    zock.bind(addr.c_str());
    LOG_INFO("Listening for requests from clients on " << addr);

    event_fd_ = eventfd(0, EFD_NONBLOCK);
    if (event_fd_ == -1)
    {
        LOG_ERROR(name_ << ": failed to create eventfd: " << strerror(errno));
        throw fungi::IOException("failed to create eventfd for ZWorkerPool",
                                 name_.c_str(),
                                 errno);
    }

    try
    {
        for (size_t i = 0; i < num_workers; ++i)
        {
            workers_.create_thread(boost::bind(&ZWorkerPool::work_,
                                               this));
        }

        // work around boost::bind not working with move semantics /
        // my inability to convince it:
        router_ = boost::thread([this,
                                 z = std::move(zock)]() mutable -> void
                                {
                                    ZWorkerPool::route_(std::move(z));
                                });
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(name_ << ": failed to create threads: " << EWHAT);
            close_();
            throw;
        });

    LOG_INFO(name_ << ": ready, " << size() << " workers");
}

ZWorkerPool::~ZWorkerPool()
{
    LOG_TRACE(name_ << ": shutting down");
    close_();
}

void
ZWorkerPool::notify_()
{
    ASSERT(event_fd_ >= 0);
    int ret;
    do
    {
        ret = eventfd_write(event_fd_, 1);
    }
    while (ret < 0 && errno == EINTR);

    VERIFY(ret == 0);
}

void
ZWorkerPool::reap_notification_()
{
    eventfd_t n = 0;

    int ret = 0;
    do
    {
        ret = eventfd_read(event_fd_, &n);
    }
    while (ret < 0 and errno == EINTR);

    if (ret < 0)
    {
        VERIFY(errno == EAGAIN);
    }
    else
    {
        VERIFY(ret == 0);
        VERIFY(n > 0);
    }
}

void
ZWorkerPool::close_()
{
    try
    {
        {
            LOCK_RX();
            LOCK_TX();

            stop_ = true;
            rx_cond_.notify_all();
            notify_();
        }

        workers_.join_all();
        router_.join();

        int ret = close(event_fd_);
        VERIFY(ret == 0);
    }
    CATCH_STD_ALL_LOG_IGNORE(name_ << ": failed to close down: " << EWHAT);
}

void
ZWorkerPool::route_(zmq::socket_t zock)
{
    pthread_setname_np(pthread_self(), "zworker_pool_r");

    {
        // transfer of ZMQ sockets between threads needs a full barrier
        LOCK_RX();
        LOG_INFO(name_ << ": entering event loop");
    }

    while (true)
    {
        try
        {
            std::array<zmq::pollitem_t, 2> pollset;
            pollset[0].socket = zock;
            pollset[0].events = ZMQ_POLLIN;
            pollset[0].revents = 0;

            pollset[1].socket = nullptr;
            pollset[1].fd = event_fd_;
            pollset[1].events = ZMQ_POLLIN;
            pollset[1].revents = 0;

            LOG_TRACE(name_ << ": starting to poll");
            const int ret = zmq::poll(pollset.data(),
                                      pollset.size());
            if (ret > 0)
            {
                LOG_TRACE(name_ << ": " << ret << " events signalled");

                if ((pollset[1].revents bitand ZMQ_POLLIN) != 0)
                {
                    send_(zock);
                }

                if ((pollset[0].revents bitand ZMQ_POLLIN) != 0)
                {
                    recv_(zock);
                }
            }
            else
            {
                LOG_WARN(name_ << ": poll unexpectedly returned " << ret);
            }

            LOCK_RX();
            if (stop_)
            {
                LOG_INFO(name_ << ": stop requested");
                break;
            }
        }
        catch (zmq::error_t& e)
        {
            if (e.num() == ETERM)
            {
                LOG_INFO(name_ << ": stop requested, breaking out of the loop");
                return;
            }
            else
            {
                LOG_ERROR(name_ << ": caught ZMQ error " << e.what() << ", num " << e.num());
            }
        }
        CATCH_STD_ALL_LOG_IGNORE(name_ << ": caught exception in router thread");
    }
}

void
ZWorkerPool::recv_(zmq::socket_t& zock)
{
    LOG_TRACE(name_);

    zmq::message_t sender_id;
    zock.recv(&sender_id);
    ZEXPECT_MORE(zock, "delimiter");

    zmq::message_t delim;
    zock.recv(&delim);

    MessageParts rx_parts(recv_parts(zock));

    const yt::DeferExecution defer = dispatch_fun_(rx_parts);
    if (defer == yt::DeferExecution::T)
    {
        LOG_TRACE(name_ << ": deferring to thread");

        LOCK_RX();
        rx_queue_.emplace_back(std::move(sender_id),
                               std::move(rx_parts));
        rx_cond_.notify_one();
    }
    else
    {
        LOG_TRACE(name_ << ": executing immediately");

        MessageParts tx_parts(worker_fun_(std::move(rx_parts)));
        ASSERT(not tx_parts.empty());

        send_(zock,
              SenderAndMessage(std::move(sender_id),
                               std::move(tx_parts)));
    }
}

void
ZWorkerPool::send_(zmq::socket_t& zock,
                   SenderAndMessage sam)
{
    zmq::message_t& sender = std::get<0>(sam);
    MessageParts& parts = std::get<1>(sam);
    ASSERT(not parts.empty());

    zock.send(sender, ZMQ_SNDMORE);

    zmq::message_t delim;
    zock.send(delim, ZMQ_SNDMORE);

    send_parts(zock,
               parts);
}

void
ZWorkerPool::send_(zmq::socket_t& zock)
{
    LOG_TRACE(name_);

    reap_notification_();

    while (true)
    {
        SenderAndMessage sam;

        {
            LOCK_TX();
            if (stop_ or tx_queue_.empty())
            {
                break;
            }

            sam = std::move(tx_queue_.front());
            tx_queue_.pop_front();
        }

        send_(zock,
              std::move(sam));
    }
}

void
ZWorkerPool::work_()
{
    pthread_setname_np(pthread_self(), "zworker_pool_w");

    while (true)
    {
        try
        {
            SenderAndMessage rx_sam;

            {
                boost::unique_lock<decltype(rx_mutex_)> u(rx_mutex_);
                rx_cond_.wait(u,
                              [&]() -> bool
                              {
                                  return not rx_queue_.empty() or stop_;
                              });

                if (stop_)
                {
                    LOG_INFO(name_ << ": stop requested, breaking out of loop");
                    break;
                }

                VERIFY(not rx_queue_.empty());
                rx_sam = std::move(rx_queue_.front());
                rx_queue_.pop_front();
            }

            MessageParts tx_parts(worker_fun_(std::move(std::get<1>(rx_sam))));
            ASSERT(not tx_parts.empty());

            LOCK_TX();

            tx_queue_.emplace_back(std::move(std::get<0>(rx_sam)),
                                   std::move(tx_parts));
            notify_();
        }
        CATCH_STD_ALL_LOG_IGNORE(name_ << ": exception in worker");
    }
}

}
