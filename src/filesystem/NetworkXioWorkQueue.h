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

#ifndef NETWORK_XIO_WORKQUEUE_H_
#define NETWORK_XIO_WORKQUEUE_H_

#include <youtils/IOException.h>

#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>

#include <boost/thread/lock_guard.hpp>
#include <youtils/SpinLock.h>

#include "NetworkXioRequest.h"

namespace volumedriverfs
{

MAKE_EXCEPTION(WorkQueueThreadsException, Exception);

class NetworkXioWorkQueue
{
public:
    NetworkXioWorkQueue(const std::string& name, EventFD& evfd_)
    : name_(name)
    , nr_threads_(0)
    , nr_queued_work(0)
    , protection_period_(5000)
    , stopping(false)
    , stopped(false)
    , evfd(evfd_)
    {
        int ret = create_workqueue_threads(1);
        if (ret < 0)
        {
            throw WorkQueueThreadsException("cannot create worker thread");
        }
    }

    ~NetworkXioWorkQueue()
    {
        shutdown();
    }

    void
    shutdown()
    {
        if (not stopped)
        {
            stopping = true;
            while (nr_threads_)
            {
                inflight_cond.notify_all();
                ::usleep(500);
            }
            stopped = true;
        }
    }

    void
    work_schedule(NetworkXioRequest *req)
    {
        queued_work_inc();
        inflight_lock.lock();
        if (need_to_grow())
        {
            create_workqueue_threads(nr_threads_ * 2);
        }
        inflight_queue.push(req);
        inflight_lock.unlock();
        inflight_cond.notify_one();
    }

    void
    queued_work_inc()
    {
        nr_queued_work++;
    }

    void
    queued_work_dec()
    {
        nr_queued_work--;
    }

    NetworkXioRequest*
    get_finished()
    {
        boost::lock_guard<decltype(finished_lock)> lock_(finished_lock);
        NetworkXioRequest *req = &finished_list.front();
        finished_list.pop_front();
        return req;
    }

    bool
    is_finished_empty()
    {
        boost::lock_guard<decltype(finished_lock)> lock_(finished_lock);
        return finished_list.empty();
    }
private:
    DECLARE_LOGGER("NetworkXioWorkQueue");

    std::string name_;
    std::atomic<size_t> nr_threads_;

    std::condition_variable inflight_cond;
    std::mutex inflight_lock;
    std::queue<NetworkXioRequest*> inflight_queue;

    mutable fungi::SpinLock finished_lock;
    boost::intrusive::list<NetworkXioRequest> finished_list;

    std::atomic<size_t> nr_queued_work;

    std::chrono::steady_clock::time_point thread_life_period_;
    uint64_t protection_period_;

    bool stopping;
    bool stopped;
    EventFD& evfd;

    void xstop_loop(NetworkXioWorkQueue *wq)
    {
        wq->evfd.writefd();
    }

    std::chrono::steady_clock::time_point
    get_time_point()
    {
        return std::chrono::steady_clock::now();
    }

    size_t
    get_max_wq_depth()
    {
        return std::thread::hardware_concurrency();
    }

    bool
    need_to_grow()
    {
        if ((nr_threads_ < nr_queued_work) &&
                (nr_threads_ * 2 <= get_max_wq_depth()))
        {
            thread_life_period_ = get_time_point() +
                std::chrono::milliseconds(protection_period_);
            return true;
        }
        return false;
    }

    bool
    need_to_shrink()
    {
        if (nr_queued_work < nr_threads_ / 2)
        {
            return thread_life_period_ <= get_time_point();
        }
        thread_life_period_ = get_time_point() +
            std::chrono::milliseconds(protection_period_);
        return false;
    }

    int
    create_workqueue_threads(size_t nr_threads)
    {
        while (nr_threads_ < nr_threads)
        {
            try
            {
                std::thread thr([&](){
                    auto fp = std::bind(&NetworkXioWorkQueue::worker_routine,
                                        this,
                                        std::placeholders::_1);
                    pthread_setname_np(pthread_self(), name_.c_str());
                    fp(this);
                });
                thr.detach();
                nr_threads_++;
            }
            catch (const std::system_error&)
            {
                LOG_ERROR("cannot create worker thread");
                return -1;
            }
        }
        return 0;
    }

    void
    worker_routine(void *arg)
    {
        NetworkXioWorkQueue *wq = reinterpret_cast<NetworkXioWorkQueue*>(arg);

        NetworkXioRequest *req;
        while (true)
        {
            std::unique_lock<std::mutex> lock_(wq->inflight_lock);
            if (wq->need_to_shrink())
            {
                wq->nr_threads_--;
                lock_.unlock();
                break;
            }
retry:
            if (wq->inflight_queue.empty())
            {
                wq->inflight_cond.wait(lock_);
                if (wq->stopping)
                {
                    wq->nr_threads_--;
                    break;
                }
                goto retry;
            }
            req = wq->inflight_queue.front();
            wq->inflight_queue.pop();
            lock_.unlock();
            wq->queued_work_dec();
            if (req->work.func)
            {
                req->work.func(&req->work);
            }
            wq->finished_lock.lock();
            wq->finished_list.push_back(*req);
            wq->finished_lock.unlock();
            xstop_loop(wq);
        }
    }
};

typedef std::shared_ptr<NetworkXioWorkQueue> NetworkXioWorkQueuePtr;

} //namespace

#endif //NETWORK_XIO_WORKQUEUE_H_
