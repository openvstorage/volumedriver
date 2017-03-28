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

#ifndef YT_PERIODIC_ACTION_POOL_H_
#define YT_PERIODIC_ACTION_POOL_H_

#include "Logging.h"
#include "PeriodicAction.h"

#include <deque>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>

namespace youtils
{

class PeriodicActionPool
    : public boost::enable_shared_from_this<PeriodicActionPool>
{
    class TaskImpl;
    using TaskImplPtr = boost::shared_ptr<TaskImpl>;
    using WeakTaskImplPtr = boost::weak_ptr<TaskImpl>;

public:
    using Ptr = boost::shared_ptr<PeriodicActionPool>;

    static Ptr
    create(const std::string& name,
           const size_t nthreads)
    {
        return Ptr(new PeriodicActionPool(name,
                                          nthreads));
    }

    ~PeriodicActionPool();

    PeriodicActionPool(const PeriodicActionPool&) = delete;

    PeriodicActionPool&
    operator=(const PeriodicActionPool&) = delete;

    class Task
    {
    public:
        ~Task();

        Task(const Task&) = delete;

        Task&
        operator=(const Task&) = delete;

        Task(Task&& other)
            : impl_(std::move(other.impl_))
            , pool_(std::move(other.pool_))
        {}

        Task&
        operator=(Task&& other)
        {
            if (this != &other)
            {
                impl_ = std::move(other.impl_);
                pool_ = std::move(other.pool_);
            }

            return *this;
        }
    private:
        friend class PeriodicActionPool;

        DECLARE_LOGGER("PeriodicActionPoolTask");

        TaskImplPtr impl_;
        Ptr pool_;

        Task(const TaskImplPtr& impl,
             const Ptr& pool)
            : impl_(impl)
            , pool_(pool)
        {}
    };

    Task
    create_task(const std::string& name,
                PeriodicAction::AbortableAction action,
                const std::atomic<uint64_t>& period,
                const bool period_in_seconds = true,
                const std::chrono::milliseconds& ramp_up = std::chrono::milliseconds(0));

private:
    DECLARE_LOGGER("PeriodicActionPool");

    boost::asio::io_service io_service_;
    decltype(io_service_)::work work_;
    const std::string name_;

    boost::mutex mutex_;
    boost::condition_variable cond_;
    bool stop_;

    std::deque<WeakTaskImplPtr> queue_;

    boost::thread_group threads_;
    boost::thread scheduler_;

    PeriodicActionPool(const std::string& name,
                       size_t nthreads);

    void
    run_service_();

    void
    run_worker_();

    void
    enqueue_(const WeakTaskImplPtr&);

    void
    stop_threads_();
};

}

#endif // !YT_PERIODIC_ACTION_POOL_H_
