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

#ifndef VFS_ZWORKER_POOL_H_
#define VFS_ZWORKER_POOL_H_

#include <deque>
#include <functional>
#include <map>
#include <vector>

#include <boost/thread.hpp>

#include <cppzmq/zmq.hpp>

#include <youtils/DeferExecution.h>
#include <youtils/BooleanEnum.h>
#include <youtils/Logging.h>
#include <youtils/Uri.h>

// * ZWorkerPool binds to a public address
// * internally it will dispatch requests to that address to a pool of worker threads
// The pool is torn down when the ZMQ context is terminated.
// TODO: push to youtils? If so, ZUtils will have to follow suit.
namespace volumedriverfs
{

class ZWorkerPool
{
public:
    using MessageParts = std::vector<zmq::message_t>;
    using WorkerFun = std::function<MessageParts(MessageParts)>;
    using DispatchFun = std::function<youtils::DeferExecution(const MessageParts&)>;

    ZWorkerPool(const std::string& name,
                zmq::context_t&,
                const youtils::Uri&,
                uint16_t num_workers,
                WorkerFun,
                DispatchFun);

    ~ZWorkerPool();

    ZWorkerPool(const ZWorkerPool&) = delete;

    ZWorkerPool&
    operator=(const ZWorkerPool&) = delete;

    size_t
    size() const
    {
        return workers_.size();
    }

private:
    DECLARE_LOGGER("ZWorkerPool");

    WorkerFun worker_fun_;
    DispatchFun dispatch_fun_;

    zmq::context_t& ztx_;
    const std::string name_;
    const youtils::Uri uri_;

    bool stop_;
    int event_fd_;

    mutable boost::mutex rx_mutex_;
    boost::condition_variable rx_cond_;

    using SenderAndMessage = std::tuple<zmq::message_t, MessageParts>;

    std::deque<SenderAndMessage> rx_queue_;

    mutable boost::mutex tx_mutex_;
    std::deque<SenderAndMessage> tx_queue_;

    boost::thread router_;
    boost::thread_group workers_;

    void
    route_(zmq::socket_t);

    void
    recv_(zmq::socket_t&);

    void
    send_(zmq::socket_t&);

    void
    send_(zmq::socket_t&,
          SenderAndMessage);

    void
    work_();

    void
    close_();

    void
    notify_();

    void
    reap_notification_();
};

}

#endif // !VFS_ZWORKER_POOL_H_
