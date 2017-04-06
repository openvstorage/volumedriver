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

#include "Assert.h"
#include "Catchers.h"
#include "PeriodicActionPool.h"

#include <boost/thread/reverse_lock.hpp>

namespace youtils
{

namespace ba = boost::asio;
namespace bs = boost::system;
namespace sc = std::chrono;

struct PeriodicActionPool::TaskImpl
    : public boost::enable_shared_from_this<TaskImpl>
{
    enum class State
    {
        Timer,
        Queue,
        Exec,
        Stopped,
    };

    TaskImpl(const std::string& name,
             PeriodicAction::AbortableAction action,
             const std::atomic<uint64_t>& period,
             const bool period_in_seconds,
             PeriodicActionPool& pool)
        : name_(name)
        , period_(period)
        , action_(std::move(action))
        , period_in_seconds_(period_in_seconds)
        , pool_(pool)
        , deadline_(pool_.io_service_)
        , stop_(false)
        , state_(State::Timer)
    {
        LOG_TRACE(name_);
    }

    ~TaskImpl()
    {
        LOG_TRACE(name_);
        ASSERT(state_ == State::Stopped);
    }

    TaskImpl(const TaskImpl&) = delete;

    TaskImpl&
    operator=(const TaskImpl&) = delete;

    TaskImpl(TaskImpl&&) = delete;

    TaskImpl&
    operator=(TaskImpl&&) = delete;

    void
    cancel()
    {
        LOG_INFO(name_ << ": cancelling");

        boost::unique_lock<decltype(lock_)> u(lock_);
        stop_ = true;

        switch (state_)
        {
        case State::Timer:
            deadline_.expires_from_now(sc::nanoseconds(0));
            // fall through
        case State::Exec:
            cond_.wait(u,
                       [&]() -> bool
                       {
                           return state_ == State::Stopped;
                       });
            break;
        case State::Queue:
            state_ = State::Stopped;
            break;
        case State::Stopped:
            break;
        }
        LOG_INFO(name_ << ": cancelled");
    }

    void
    stop_and_notify()
    {
        LOG_TRACE(name_);
        stop_ = true;
        state_ = State::Stopped;
        cond_.notify_one();
    }

    void
    run()
    {
        LOG_TRACE(name_);

        boost::unique_lock<decltype(lock_)> g(lock_);
        PeriodicActionContinue cont = PeriodicActionContinue::T;

        if (state_ == State::Queue)
        {
            state_ = State::Exec;
            boost::reverse_lock<decltype(g)> r(g);

            try
            {
                cont = action_();
            }
            CATCH_STD_ALL_LOG_IGNORE(name_ << ": caught exception");
        }
        else
        {
            ASSERT(state_ == State::Stopped);
            ASSERT(stop_);
        }

        if (cont == PeriodicActionContinue::F or stop_)
        {
            stop_and_notify();
        }
        else
        {
            state_ = State::Timer;
            schedule(period_in_seconds_ ?
                     sc::milliseconds(period_ * 1000) :
                     sc::milliseconds(period_));
        }
    }

    void
    schedule(const sc::milliseconds& timeout)
    {
        auto fun([this, self = shared_from_this()](const bs::error_code& ec)
                 {
                     LOG_TRACE(name_ << ": " << ec);
                     boost::lock_guard<decltype(lock_)> g(lock_);

                     ASSERT(state_ == State::Timer);

                     bool stop = stop_;

                     if (ec == bs::errc::operation_canceled or
                         ec == ba::error::operation_aborted)
                     {
                         LOG_INFO(name_ << ": timer cancelled");
                         stop = true;
                     }
                     else
                     {
                         if (ec)
                         {
                             LOG_ERROR(name_ << ": timer reported error " <<
                                       ec.message());
                             stop = true;
                         }
                         else
                         {
                             LOG_TRACE(name_ << ": timer fired");
                         }
                     }

                     if (not stop)
                     {
                         pool_.enqueue_(self);
                         state_ = State::Queue;
                         cond_.notify_one();
                     }
                     else
                     {
                         stop_and_notify();
                     }
                 });

        LOG_TRACE(name_);

        deadline_.expires_from_now(timeout);
        deadline_.async_wait(std::move(fun));
    }

    DECLARE_LOGGER("PeriodicActionPoolTaskImpl");

    std::string name_;
    const std::atomic<uint64_t>& period_;
    PeriodicAction::AbortableAction action_;
    bool period_in_seconds_;
    PeriodicActionPool& pool_;
    ba::steady_timer deadline_;
    boost::mutex lock_;
    boost::condition_variable cond_;
    bool stop_;
    State state_;
};

PeriodicActionPool::Task::~Task()
{
    if (impl_)
    {
        try
        {
            impl_->cancel();
        }
        CATCH_STD_ALL_LOG_IGNORE("Failed to cancel task " << impl_->name_);
    }
}

PeriodicActionPool::PeriodicActionPool(const std::string& name,
                                       size_t nthreads)
    : work_(io_service_)
    , name_(name)
    , stop_(false)
{
    THROW_WHEN(nthreads == 0);

    try
    {
        for (size_t i = 0; i < nthreads; ++i)
        {
            threads_.create_thread(boost::bind(&PeriodicActionPool::run_worker_,
                                               this));
        }

        LOG_INFO(name_ << ": starting service thread");

        scheduler_ = boost::thread(boost::bind(&PeriodicActionPool::run_service_,
                                               this));

        LOG_INFO(name_ << ": alive and well");
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(name_ << ": failed to create threads: " << EWHAT);
            stop_threads_();
            throw;
        });
}

PeriodicActionPool::~PeriodicActionPool()
{
    try
    {
        stop_threads_();
    }
    CATCH_STD_ALL_LOG_IGNORE(name_ << ": failed to stop");
}

void
PeriodicActionPool::stop_threads_()
{
    LOG_INFO(name_ << ": stopping");

    {
        boost::lock_guard<decltype(mutex_)> g(mutex_);
        stop_ = true;
        cond_.notify_all();
    }

    io_service_.stop();
    scheduler_.join();
    threads_.join_all();
}

void
PeriodicActionPool::run_service_()
{
    pthread_setname_np(pthread_self(), "periodic_act_s");

    while (true)
    {
        try
        {
            io_service_.run();
            LOG_INFO(name_ << ": I/O service exited");
            return;
        }
        CATCH_STD_ALL_LOG_IGNORE(name_ << ": caught exception in service thread");
    }
}

void
PeriodicActionPool::run_worker_()
{
    pthread_setname_np(pthread_self(), "periodic_act_w");

    while (true)
    {
        TaskImplPtr task;

        {
            boost::unique_lock<decltype(mutex_)> u(mutex_);
            cond_.wait(u,
                       [&]() -> bool
                       {
                           return not queue_.empty() or stop_;
                       });

            if (stop_)
            {
                break;
            }

            if (not queue_.empty())
            {
                task = queue_.front().lock();
                queue_.pop_front();
            }
        }

        if (task)
        {
            task->run();
        }
    }
}

PeriodicActionPool::Task
PeriodicActionPool::create_task(const std::string& name,
                                PeriodicAction::AbortableAction action,
                                const std::atomic<uint64_t>& period,
                                const bool period_in_seconds,
                                const sc::milliseconds& ramp_up)
{
    auto impl(boost::make_shared<TaskImpl>(name,
                                           std::move(action),
                                           period,
                                           period_in_seconds,
                                           *this));
    Task task(impl,
              shared_from_this());
    impl->schedule(ramp_up);
    return task;
}


void
PeriodicActionPool::enqueue_(const WeakTaskImplPtr& task)
{
    boost::lock_guard<decltype(mutex_)> g(mutex_);
    queue_.emplace_back(task);
    cond_.notify_one();
}

}
