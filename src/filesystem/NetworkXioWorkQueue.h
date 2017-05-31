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

#include "NetworkXioRequest.h"

#include <boost/thread/lock_guard.hpp>

#include <youtils/InlineExecutor.h>
#include <youtils/IOException.h>
#include <youtils/SpinLock.h>

#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <queue>

namespace volumedriverfs
{

MAKE_EXCEPTION(WorkQueueThreadsException, Exception);

class NetworkXioWorkQueue
{
public:
    NetworkXioWorkQueue(const std::string& name,
                        EventFD& evfd_,
                        fungi::SpinLock& finished_lock_,
                        boost::intrusive::list<NetworkXioRequest>& fl_,
                        unsigned int wq_max_threads)
    : name_(name)
    , nr_threads_(0)
    , finished_lock(finished_lock_)
    , finished_list(fl_)
    , nr_queued_work(0)
    , protection_period_(60000)
    , stopping(false)
    , stopped(false)
    , evfd(evfd_)
    , max_threads(wq_max_threads)
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
    work_schedule(const NetworkXioRequestPtr& req)
    {
        queued_work_inc();
        inflight_lock.lock();
        size_t new_nr_threads = need_to_grow();
        if (new_nr_threads > 0)
        {
            create_workqueue_threads(new_nr_threads);
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

    NetworkXioRequestPtr
    get_finished()
    {
        boost::lock_guard<decltype(finished_lock)> lock_(finished_lock);
        if (not finished_list.empty())
        {
            // Don't bump the refcount, that is done before pushing
            // to the list.
            auto req(NetworkXioRequestPtr(&finished_list.front(),
                                          false));
            finished_list.pop_front();
            return req;
        }
        else
        {
            return nullptr;
        }
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
    std::queue<NetworkXioRequestPtr> inflight_queue;

    fungi::SpinLock& finished_lock;
    boost::intrusive::list<NetworkXioRequest>& finished_list;

    std::atomic<size_t> nr_queued_work;

    std::chrono::steady_clock::time_point thread_life_period_;
    uint64_t protection_period_;

    bool stopping;
    bool stopped;
    EventFD& evfd;
    unsigned int max_threads;

    void xstop_loop()
    {
        evfd.writefd();
    }

    std::chrono::steady_clock::time_point
    get_time_point()
    {
        return std::chrono::steady_clock::now();
    }

    unsigned int
    get_max_wq_depth()
    {
        if (max_threads == 0)
        {
            return std::thread::hardware_concurrency();
        }
        else
        {
            return max_threads;
        }
    }

    size_t
    need_to_grow()
    {
        if (nr_threads_ >= nr_queued_work)
        {
            return 0;
        }

        size_t roof = get_max_wq_depth();
        if (nr_threads_ >= roof)
        {
            return 0;
        }

        thread_life_period_ = get_time_point() +
            std::chrono::milliseconds(protection_period_);
        return std::min(nr_threads_ * 2, roof);
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
                                        this);
                    pthread_setname_np(pthread_self(), name_.c_str());
                    fp();
                });
                thr.detach();
                nr_threads_++;
            }
            catch (const std::system_error& e)
            {
                LOG_ERROR("cannot create worker thread: " << e.what());
                return -1;
            }
        }
        return 0;
    }

    void
    worker_routine()
    {
        while (true)
        {
            NetworkXioRequestPtr req;
            std::unique_lock<std::mutex> lock_(inflight_lock);
            if (need_to_shrink())
            {
                nr_threads_--;
                lock_.unlock();
                break;
            }
retry:
            if (inflight_queue.empty())
            {
                inflight_cond.wait(lock_);
                if (stopping)
                {
                    nr_threads_--;
                    break;
                }
                goto retry;
            }
            req = inflight_queue.front();
            inflight_queue.pop();
            lock_.unlock();
            queued_work_dec();

            boost::future<NetworkXioRequest&> future;

            if (not req->work.is_ctrl and req->work.func)
            {
                future = req->work.func();
                if (req->work.is_ctrl)
                {
                    ASSERT(not future.valid());
                    req->work.dispatch_ctrl_request(req);
                    continue;
                }
            }
            if (req->work.is_ctrl and req->work.func_ctrl)
            {
                ASSERT(not future.valid());
                future = req->work.func_ctrl();
            }

            ASSERT(future.valid());
            ASSERT(not req->future.valid());

            // Lifetime:
            // The object behind `req' is kept alive by capturing a(n owning)
            // reference in the closure and `req' itself.
            // Since the closure outlives its invocation (it's captured by the
            // member future), the reference is released explicitly in the lambda
            // by resetting the intrusive_ptr.
            req->future = future.then(youtils::InlineExecutor::get(),
                                      [r = req,
                                       this](boost::future<NetworkXioRequest&> f) mutable -> void
                                      {
                                          ASSERT(not f.has_exception());
                                          ASSERT(f.is_ready());

                                          NetworkXioRequest& req = f.get();
                                          ASSERT(&req == r.get());

                                          // Bump the refcount before appending it to the
                                          // finished_list.
                                          intrusive_ptr_add_ref(&req);
                                          r = nullptr;

                                          bool wakeup = false;
                                          {
                                              boost::lock_guard<decltype(finished_lock)>
                                                  g(finished_lock);
                                              wakeup = finished_list.empty();
                                              finished_list.push_back(req);
                                          }

                                          if (wakeup)
                                          {
                                              xstop_loop();
                                          }
                                      });
        }
    }
};

typedef std::shared_ptr<NetworkXioWorkQueue> NetworkXioWorkQueuePtr;

} //namespace

#endif //NETWORK_XIO_WORKQUEUE_H_
