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

#include <future>

#include <boost/lexical_cast.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>

namespace volumedriverfs
{

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

class ZWorker
{
    typedef ZWorkerPool::ZWorkerId ZWorkerId;

public:
    ZWorker(zmq::context_t& ztx,
            const ZWorkerId zwid,
            const std::string pool_addr,
            ZWorkerPool::WorkerFun& fun)
        : ztx_(ztx)
        , pool_addr_(pool_addr)
        , fun_(fun)
        , id_(zwid)
    {
        std::future<bool> future(initialized_.get_future());

        thread_ = boost::thread([&] { work_(); });

        try
        {
            future.get();
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(id() << ": failed to initialize thread: " << EWHAT);
                thread_.join();
                throw;
            });

        LOG_INFO(id() << ": up and running");
    }

    ~ZWorker()
    {
        try
        {
            thread_.join();
        }
        CATCH_STD_ALL_LOG_IGNORE(id() << ": failed to join");

        LOG_INFO(id() << ": and that was that");
    }

    ZWorker(const ZWorker&) = delete;

    ZWorker&
    operator=(const ZWorker&) = delete;

    ZWorkerId
    id() const
    {
        return id_;
    }

private:
    DECLARE_LOGGER("ZWorker");

    boost::thread thread_;
    zmq::context_t& ztx_;
    const std::string pool_addr_;
    ZWorkerPool::WorkerFun& fun_;
    std::promise<bool> initialized_;
    const ZWorkerId id_;

    // The socket isn't a member to prevent multithreaded use of it.

    std::unique_ptr<zmq::socket_t>
    create_socket_()
    {
        LOG_TRACE(id() << ": creating new socket");

        std::unique_ptr<zmq::socket_t> zock(new zmq::socket_t(ztx_, ZMQ_REQ));
        ZUtils::socket_no_linger(*zock);

        const ZWorkerId sock_id = id();
        zock->setsockopt(ZMQ_IDENTITY, &sock_id, sizeof(sock_id));

        LOG_INFO(id() << ": connecting socket to pool " << pool_addr_);
        zock->connect(pool_addr_.c_str());

        registerize_(*zock);

        return zock;
    }

    void
    registerize_(zmq::socket_t& zock)
    {
        LOG_TRACE(id() << ": registering with pool");
        zmq::message_t msg(0);
        zock.send(msg, 0);
    }

    std::unique_ptr<zmq::socket_t>
    recreate_socket_(std::unique_ptr<zmq::socket_t> zock)
    {
        LOG_TRACE(id() << ": recreating socket");

        // returns an error each time anyway

        // try
        // {
        //     zock->disconnect(pool_addr_.c_str());
        // }
        // CATCH_STD_ALL_LOG_IGNORE(id() << ": failed to disconnect from " << pool_addr_);
        zock.reset();

        return create_socket_();
    }

    void
    work_()
    {
        try
        {
            do_work_();
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(id() << ": error in worker: " << EWHAT - " exiting thread");
            });
    }

    void
    do_work_()
    {
        LOG_TRACE(id());

        std::unique_ptr<zmq::socket_t> zock;

        try
        {
            zock = create_socket_();
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR(id() << ": failed to create socket: " << EWHAT);
                initialized_.set_exception(std::current_exception());
                throw;
            });

        initialized_.set_value(true);

        while (true)
        {
            try
            {
                zmq::message_t sender_id;
                zock->recv(&sender_id);

                if (sender_id.size() == 0)
                {
                    ZEXPECT_NOTHING_MORE(*zock);
                    LOG_INFO(id() << ": the pool has asked us to stop");
                    break;
                }
                else
                {
                    ZEXPECT_MORE(*zock, "delimiter");
                    zmq::message_t delimiter;
                    zock->recv(&delimiter);

                    ZWorkerPool::MessageParts parts_in(recv_parts(*zock));
                    if (parts_in.empty())
                    {
                        LOG_ERROR(id() << ": expected more message parts");
                        throw fungi::IOException("Expected more message parts",
                                                 boost::lexical_cast<std::string>(id()).c_str(),
                                                 EINVAL);
                    }

                    LOG_TRACE(id() << ": handing off to worker fun");

                    ZWorkerPool::MessageParts parts_out(fun_(std::move(parts_in)));
                    VERIFY(not parts_out.empty());

                    LOG_TRACE(id() << ": sending reply");

                    zock->send(sender_id, ZMQ_SNDMORE);
                    zock->send(delimiter, ZMQ_SNDMORE);
                    send_parts(*zock, parts_out);
                }
            }
            catch (zmq::error_t& e)
            {
                if (e.num() == ETERM)
                {
                    LOG_INFO(id() << ": termination requested");
                    break;
                }
                else
                {
                    LOG_ERROR(id() << ": caught ZMQ exception " << e.what() <<
                              ", num " << e.num());
                    zock = recreate_socket_(std::move(zock));
                }
            }
            CATCH_STD_ALL_EWHAT({
                    LOG_ERROR(id() << ": caught exception: " << EWHAT);
                    registerize_(*zock);
                });
        }
    }
};

#define LOCK()                                          \
    boost::lock_guard<decltype(lock_)> lg__(lock_)

ZWorkerPool::ZWorkerPool(const std::string& name,
                         zmq::context_t& ztx,
                         const std::string& pub_addr,
                         WorkerFun worker_fun,
                         uint16_t min_workers,
                         uint16_t max_workers)
    : min_(min_workers)
    , max_(max_workers)
    , worker_fun_(std::move(worker_fun))
    , ztx_(ztx)
    , name_(name)
    , pub_addr_(pub_addr)
{
    validate_settings(min_, max_);

    LOG_INFO(name_ << ": ready, min workers " << min_ << ", max workers " << max_);

    std::future<bool> future(initialized_.get_future());

    router_thread_ = boost::thread([&]{ route_(); });

    try
    {
        future.get();
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(name_ << ": failed to initialize router thread: " << EWHAT);
            router_thread_.join();
            throw;
        });
}

ZWorkerPool::~ZWorkerPool()
{
    LOG_TRACE(name_ << ": shutting down");

    try
    {
        router_thread_.join();
    }
    CATCH_STD_ALL_LOG_IGNORE("failed to shut down router thread properly - resources might be leaked");

    busy_ones_.clear();
    unused_ones_.clear();
}

void
ZWorkerPool::validate_settings(uint16_t min,
                               uint16_t max)
{
    // XXX: enforce upper boundaries (which ones?) too?
    if (min == 0)
    {
        LOG_ERROR(name_ << ": minimum number of workers must be > 0");
        throw fungi::IOException("Minimum number of workers must be > 0",
                                 name_.c_str(),
                                 EINVAL);
    }

    if (max < min)
    {
        LOG_ERROR(name_ <<
                  ": maximum number of workers must not be < minimum number of workers");
        throw fungi::IOException("Maximum number of workers must not be < minimum number of workers",
                                 name_.c_str(),
                                 EINVAL);
    }
}

std::string
ZWorkerPool::back_address_() const
{
    return "inproc://" + name_ + "-back";
}

void
ZWorkerPool::route_()
{
    bool up = false;
    LOG_TRACE("kicking off");

    try
    {
        zmq::socket_t front_sock(ztx_, ZMQ_ROUTER);
        ZUtils::socket_no_linger(front_sock);

        LOG_INFO("Binding public interface to " << pub_addr_);
        front_sock.bind(pub_addr_.c_str());
        LOG_TRACE("Listening for requests from clients on " << pub_addr_);

        zmq::socket_t back_sock(ztx_, ZMQ_ROUTER);

        ZUtils::socket_no_linger(back_sock);

        LOG_INFO("Binding interface to workers to " << back_address_());
        back_sock.bind(back_address_().c_str());

        LOG_TRACE("Listening for messages from workers on " << back_address_());

        initialized_.set_value(true);
        up = true;

        CanMakeProgress can_make_progress = CanMakeProgress::T;

        while (true)
        {
            try
            {
                std::array<zmq::pollitem_t, 2> pollset;
                pollset[0].socket = front_sock;
                pollset[0].events = (can_make_progress == CanMakeProgress::T) ?
                    ZMQ_POLLIN :
                    0;
                pollset[0].revents = 0;

                pollset[1].socket = back_sock;
                pollset[1].events = ZMQ_POLLIN;
                pollset[1].revents = 0;

                LOG_TRACE(name_ << ": starting to poll");
                const int ret = zmq::poll(pollset.data(), pollset.size());
                if (ret > 0)
                {
                    LOG_TRACE(name_ << ": " << ret << " events signalled");

                    if ((pollset[1].revents bitand ZMQ_POLLIN) != 0)
                    {
                        handle_back_(front_sock, back_sock);
                        can_make_progress = CanMakeProgress::T;
                    }

                    if ((pollset[0].revents bitand ZMQ_POLLIN) != 0)
                    {
                        can_make_progress = handle_front_(front_sock, back_sock);
                    }
                }
                else
                {
                    LOG_WARN(name_ << ": poll unexpectedly returned " << ret);
                    can_make_progress = CanMakeProgress::T;
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
                    LOG_ERROR("Caught ZMQ error " << e.what() << ", num " << e.num());
                    // can we really proceed here, or do we need to reset the sockets?
                    can_make_progress = CanMakeProgress::T;
                }
            }
        }
    }
    CATCH_STD_ALL_EWHAT({
            if (not up)
            {
                initialized_.set_exception(std::current_exception());
            }
            LOG_ERROR(name_ << ": " << EWHAT << " - exiting router thread");
        });
}

CanMakeProgress
ZWorkerPool::handle_front_(zmq::socket_t& front_sock,
                           zmq::socket_t& back_sock)
{
    LOG_TRACE(name_);

    ZWorkerId id = 0;

    {
        LOCK();

        VERIFY(unused_ones_.size() + busy_ones_.size() <= max_);

        if (not unused_ones_.empty())
        {
            ZWorkerPtr w(std::move(unused_ones_.front()));
            LOG_TRACE(name_ << ": using idle zworker " << w->id());

            unused_ones_.pop_front();
            auto res(busy_ones_.emplace(std::make_pair(w->id(), std::move(w))));
            VERIFY(res.second);
            id = res.first->first;
        }
        else if (busy_ones_.size() == max_)
        {
            LOG_WARN(name_ << ": max number of workers " << max_ <<
                     " reached, we need to wait for one of the existing workers to become available");
            return CanMakeProgress::F;
        }
        else
        {
            ZWorkerPtr w(new ZWorker(ztx_,
                                     next_zworker_id_(),
                                     back_address_(),
                                     worker_fun_));

            LOG_INFO(name_ << ": waiting for newly spun up zworker " << w->id());

            auto res(busy_ones_.emplace(std::make_pair(w->id(), std::move(w))));
            VERIFY(res.second);

            return CanMakeProgress::F;
        }
    }

    // cf. next_worker_id_()
    ASSERT(id != 0);

    zmq::message_t recipient(sizeof(id));
    memcpy(recipient.data(), &id, sizeof(id));

    zmq::message_t delim(0);

    MessageParts parts(recv_parts(front_sock));

    THROW_WHEN(parts.empty());
    // would clash with our internal protocol and indicate a ZMQ bug
    VERIFY(parts[0].size() != 0);

    back_sock.send(recipient, ZMQ_SNDMORE);
    back_sock.send(delim, ZMQ_SNDMORE);
    send_parts(back_sock, parts);

    return CanMakeProgress::T;
}

void
ZWorkerPool::handle_back_(zmq::socket_t& front_sock,
                          zmq::socket_t& back_sock)
{
    LOG_TRACE(name_);

    zmq::message_t worker_id;
    back_sock.recv(&worker_id);

    VERIFY(worker_id.size() == sizeof(ZWorkerId));

    const ZWorkerId id = *static_cast<ZWorkerId*>(worker_id.data());

    ZEXPECT_MORE(back_sock, "delimiter");

    zmq::message_t delimiter;
    back_sock.recv(&delimiter);

    MessageParts parts(recv_parts(back_sock));
    VERIFY(not parts.empty());

    if (parts[0].size() == 0)
    {
        LOG_INFO(name_ << ": worker " << id << " reported for duty");
        worker_ready_(id);
    }
    else
    {
        send_parts(front_sock, parts);
        worker_done_(id, back_sock);
    }
}

ZWorkerPool::ZWorkerId
ZWorkerPool::next_zworker_id_()
{
    // ZMQWorkerTest.{exceptional_work,stress} occasionally got stuck after the socket
    // was reset and socket identities were based on the object's address. I *suspect*
    // that this might have been caused by ZMQs destruction scheme (objects are handed
    // to a reaper thread) leading to conflicts with recycled socket identities.
    // This simple scheme avoids that for the most part (and the hangs are gone, so
    // there might be something to my theory); the other piece to the puzzle is *not*
    // to recreate a socket with the same identity as part of the error handling, but
    // to simply reuse the existing socket if it's not the source of the exception.
    const ZWorkerId id = next_id_;
    next_id_ += 2;

    ASSERT((id % 2) != 0);

    // ZMQ socket identities starting with binary 0 are reserved.
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    VERIFY((id bitand 1ULL) != 0);
#else
#error "fix this for big endian platforms"
#endif

    return id;
}

void
ZWorkerPool::worker_ready_(ZWorkerId id)
{
    LOG_TRACE(name_ << ": worker is ready");
    LOCK();

    auto it = busy_ones_.find(id);
    VERIFY(it != busy_ones_.end());

    VERIFY(unused_ones_.size() + busy_ones_.size() <= max_);
    unused_ones_.emplace_front(std::move(it->second));
    busy_ones_.erase(it);
}

void
ZWorkerPool::worker_done_(ZWorkerId id,
                          zmq::socket_t& back_sock)
{
    LOG_TRACE(name_ << ": worker " << id << " finished");

    LOCK();

    auto it = busy_ones_.find(id);
    VERIFY(it != busy_ones_.end());

    VERIFY(unused_ones_.size() + busy_ones_.size() <= max_);

    LOG_TRACE(name_ << ": unused " << unused_ones_.size() <<
              ", busy " << busy_ones_.size());

    if (unused_ones_.size() + busy_ones_.size() <= min_)
    {
        LOG_TRACE(name_ << ": returning zworker " << it->second->id() << " to pool");
        unused_ones_.emplace_front(std::move(it->second));
    }
    else
    {
        LOG_TRACE(name_ << ": stopping zworker " << it->second->id());
        stop_worker_(id, back_sock);
    }

    // XXX: this one might take too long as we're waiting for the thread to exit
    busy_ones_.erase(it);
}

void
ZWorkerPool::stop_worker_(ZWorkerId id,
                          zmq::socket_t& back_sock)
{
    LOG_INFO(name_ << ": asking worker " << id << " to stop");
    zmq::message_t worker(sizeof(id));
    *static_cast<ZWorkerId*>(worker.data()) = id;

    back_sock.send(worker, ZMQ_SNDMORE);

    zmq::message_t delimiter(0);
    back_sock.send(delimiter, ZMQ_SNDMORE);

    zmq::message_t msg(0);
    back_sock.send(delimiter, 0);
}

void
ZWorkerPool::resize(uint16_t min, uint16_t max)
{
    validate_settings(min, max);

    LOCK();

    min_ = min;
    max_ = max;
}

}
