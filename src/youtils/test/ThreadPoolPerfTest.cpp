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

#include "../InitializedParam.h"
#include "../System.h"
#include <gtest/gtest.h>
#include "../ThreadPool.h"
#include "../wall_timer.h"

#include <atomic>
#include <condition_variable>

#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>

namespace initialized_params
{

const char perf_threadpool_test_name[] = "perf_test_threadpool";

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(perf_threadpool_test_threads,
                                                  uint32_t);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(perf_threadpool_test_threads,
                                      perf_threadpool_test_name,
                                      "num_threads",
                                      "Number of threads",
                                      ShowDocumentation::T,
                                      4);
}

namespace youtilstest
{

using namespace youtils;

namespace
{

namespace ip = initialized_params;

struct PerfTestThreadPoolTraits
{
    static const bool
    requeue_before_first_barrier_on_error = true;

    static const bool
    may_reorder = true;

    static const char* component_name;

    typedef PARAMETER_TYPE(ip::perf_threadpool_test_threads) number_of_threads_type;

    const static uint32_t max_number_of_threads = 32;

    static uint64_t
    sleep_microseconds_if_queue_is_inactive()
    {
        return 10000ULL;
    }

    static uint64_t
    wait_microseconds_before_retry_after_error(uint32_t /* errorcount */)
    {
        return 10000;
    }

    static int
    default_producer_id()
    {
        return 0;
    }
};

const char* PerfTestThreadPoolTraits::component_name =
    ip::perf_threadpool_test_name;

typedef int queue_id_type;

typedef ThreadPool<queue_id_type,
                   PerfTestThreadPoolTraits> threadpool_type;

struct Callback
{
    Callback(size_t count, uint64_t delay_us)
        : lock_()
        , cond_()
        , count_(count)
        , delay_us_(delay_us)
    {}

    void
    operator()()
    {
        if (delay_us_ != 0)
        {
            boost::this_thread::sleep(boost::posix_time::microseconds(delay_us_));
        }

        if (--count_ == 0)
        {
            std::lock_guard<lock_type> g(lock_);
            cond_.notify_one();
        }
    }

    typedef std::mutex lock_type;
    lock_type lock_;

    std::condition_variable cond_;

    std::atomic<uint64_t> count_;
    const uint64_t delay_us_;
};

class PerfTestTask
    : public threadpool_type::Task
{
public:
    explicit PerfTestTask(Callback& done,
                          queue_id_type queue_id = 0,
                          BarrierTask barrier = BarrierTask::F)
        : threadpool_type::Task(barrier)
        , done_(done)
        , queue_id_(queue_id)
    {}

    virtual const threadpool_type::Task::Producer_t&
    getProducerID() const
    {
        return queue_id_;
    }

    virtual void
    run(int /* thread_id */)
    {
        done_();
    }

    virtual const std::string&
    getName() const
    {
        static const std::string s("PerfTestTask");
        return s;
    }

private:
    Callback& done_;
    const queue_id_type queue_id_;
};

typedef std::shared_ptr<PerfTestTask> perf_test_task_ptr;

class SleeperTask
    : public threadpool_type::Task
{
public:
    explicit SleeperTask(std::shared_ptr<std::mutex> m,
                         queue_id_type queue_id = 0)
        : threadpool_type::Task(BarrierTask::F)
        , lock_(m)
        , done_(false)
        , queue_id_(queue_id)
    {}

    virtual const threadpool_type::Task::Producer_t&
    getProducerID() const
    {
        return queue_id_;
    }

    virtual const std::string&
    getName() const
    {
        static const std::string s("SleeperTask");
        return s;
    }

    virtual void
    run(int /* thread_id */)
    {
        std::unique_lock<std::mutex> u(*lock_);
        while (not done_)
        {
            cond_.wait(u);
        }
    }

    void
    wakeup()
    {
        std::lock_guard<std::mutex> g(*lock_);
        done_ = true;
        cond_.notify_one();
    }

private:
    std::shared_ptr<std::mutex> lock_;
    std::condition_variable cond_;
    bool done_;
    const queue_id_type queue_id_;
};

class BarrierTask
    : public threadpool_type::Task
{
public:
    explicit BarrierTask(queue_id_type queue_id = 0)
        : threadpool_type::Task(youtils::BarrierTask::T)
        , queue_id_(queue_id)
    {}

    virtual const threadpool_type::Task::Producer_t&
    getProducerID() const
    {
        return queue_id_;
    }

    virtual const std::string&
    getName() const
    {
        static const std::string s("BarrierTask");
        return s;
    }

    virtual void
    run(int /* thread_id */)
    {}

private:
    const queue_id_type queue_id_;
};

class Blocker
{
public:
    explicit Blocker(threadpool_type& tp,
                     queue_id_type queue_id = 0)
        : tp_(tp)
        , lock_(new std::mutex())
        , sleeper_(new SleeperTask(lock_, queue_id))
    {
        std::lock_guard<std::mutex> g(*lock_);

        tp_.addTask(sleeper_);
        std::shared_ptr<BarrierTask> b(new BarrierTask(queue_id));
        tp_.addTask(new BarrierTask(queue_id));
    }

    ~Blocker()
    {
        sleeper_->wakeup();
    }

private:
    threadpool_type& tp_;
    std::shared_ptr<std::mutex> lock_;
    SleeperTask* sleeper_;
};

struct
nobarriers
{
    youtils::BarrierTask
    operator()(uint64_t /* task */,
               unsigned /* queue */)
    {
        return youtils::BarrierTask::F;
    }
};

struct
allbarriers
{
    youtils::BarrierTask
    operator()(uint64_t /* task */,
               unsigned /* queue */)
    {
        return youtils::BarrierTask::T;
    }
};

}

class ThreadPoolPerfTest
    : public testing::Test
{
public:
    ThreadPoolPerfTest()
        : num_threads_(youtils::System::get_env_with_default("THREADPOOL_THREADS", 1U))
        , tasks_per_queue_(youtils::System::get_env_with_default("TASKS_PER_QUEUE", 1ULL << 20))
        , num_queues_(youtils::System::get_env_with_default("NUM_QUEUES", 1U))
        , idle_queues_(youtils::System::get_env_with_default("IDLE_QUEUES", 0U))
        , delay_us_(youtils::System::get_env_with_default("DELAY_US", 0ULL))
    {
        EXPECT_LT(0U, num_threads_);
    }

    typedef std::shared_ptr<Blocker> blocker_ptr;
    typedef std::vector<blocker_ptr> blocker_ptr_vec;

    typedef std::shared_ptr<Callback> callback_ptr;
    typedef std::vector<callback_ptr> callback_ptr_vec;

    template<typename barrier_inserter>
    std::pair<double, double>
    run_test(barrier_inserter& insert_barrier,
             bool prefill,
             uint64_t tasks_per_queue,
             unsigned num_queues,
             unsigned num_threads,
             uint64_t delay_us,
             unsigned idle_queues)
    {
        EXPECT_LT(0U, tasks_per_queue);
        EXPECT_LT(0U, num_queues);
        EXPECT_LE(0U, idle_queues);

        boost::property_tree::ptree pt;
        PARAMETER_TYPE(ip::perf_threadpool_test_threads)(num_threads).persist(pt);
        pt.put("version", 1);
        std::unique_ptr<threadpool_type> tp(new threadpool_type(pt));

        BOOST_SCOPE_EXIT_TPL((&tp))
        {
            EXPECT_NO_THROW(tp->stop()) << "Failed to stop threadpool";
        }
        BOOST_SCOPE_EXIT_END;

        {
            blocker_ptr_vec blockers(idle_queues);

            for (size_t i = 0; i < idle_queues; ++i)
            {
                blockers[i] = blocker_ptr(new Blocker(*tp, num_queues + i));
            }
        }

        callback_ptr_vec callbacks(num_queues);

        for (size_t i = 0; i < callbacks.size(); ++i)
        {
            callbacks[i] = callback_ptr(new Callback(tasks_per_queue, delay_us));
        }

        youtils::wall_timer t;

        double post_time;

        if (prefill)
        {
            blocker_ptr_vec blockers(num_queues);
            for (size_t i = 0; i < blockers.size(); ++i)
            {
                blockers[i] = blocker_ptr(new Blocker(*tp, i));
            }

            post_time = post_tasks_(insert_barrier, *tp, callbacks, tasks_per_queue);
            t.restart();
        }
        else
        {
            post_time = post_tasks_(insert_barrier, *tp, callbacks, tasks_per_queue);
        }

        for (size_t i = 0; i < callbacks.size(); ++i)
        {
            callback_ptr cb = callbacks[i];
            std::unique_lock<Callback::lock_type> u(cb->lock_);
            while (cb->count_ > 0)
            {
                ASSERT(cb->count_ <= tasks_per_queue);
                cb->cond_.wait(u);
            }
        }

        const double proc_time = t.elapsed();

        std::cout <<
            "# queues: " << num_queues <<
            ", tasks per queue: " << tasks_per_queue <<
            ", # idle queues: " << idle_queues <<
            ", threads in pool: " << tp->getNumThreads() <<
            ", delay per task (us): " << delay_us <<
            ", processing duration (s): " << proc_time <<
            std::endl;

        return std::make_pair(post_time, proc_time);
    }

    template<typename barrier_inserter>
    void
    run_tests(barrier_inserter& insert_barrier,
              bool prefill,
              uint64_t tasks_per_queue,
              unsigned num_queues,
              unsigned num_threads,
              uint64_t delay_us,
              unsigned idle_queues)
    {
        typedef std::pair<double, double> timings;
        typedef std::map<unsigned, timings> qresults;
        typedef std::map<unsigned, qresults> tresults;

        tresults res;

        for (unsigned t = 1; t <= num_threads; t *= 2)
        {
            qresults qr;

            for (unsigned q = 1; q <= num_queues; q *= 2)
            {
                qr[q] = run_test(insert_barrier,
                                 prefill,
                                 tasks_per_queue / q,
                                 q,
                                 t,
                                 delay_us,
                                 idle_queues);
            }

            res[t] = std::move(qr);
        }

        std::cout << "post timings" << std::endl;

        BOOST_FOREACH(const tresults::value_type& tv, res)
        {
            const qresults& qres = tv.second;

            BOOST_FOREACH(const qresults::value_type& qv, qres)
            {
                std::cout << qv.second.first << ",";
            }
            std::cout << std::endl;
        }

        std::cout << "proc timings" << std::endl;

        BOOST_FOREACH(const tresults::value_type& tv, res)
        {
            const qresults& qres = tv.second;

            BOOST_FOREACH(const qresults::value_type& qv, qres)
            {
                std::cout << qv.second.second << ",";
            }
            std::cout << std::endl;
        }
    }

protected:
    unsigned long num_threads_;
    uint64_t tasks_per_queue_;
    unsigned num_queues_;
    unsigned idle_queues_;
    uint64_t delay_us_;

    DECLARE_LOGGER("ThreadPoolPerfTest");

private:

    template<typename barrier_inserter>
    double
    post_tasks_(barrier_inserter& insert_barrier,
                threadpool_type& tp,
                callback_ptr_vec& callbacks,
                uint64_t tasks_per_queue)
    {
        youtils::wall_timer t;

        for (uint64_t i = 0; i < tasks_per_queue; ++i)
        {
            for (size_t j = 0; j < callbacks.size(); ++j)
            {
                tp.addTask(new PerfTestTask(*callbacks[j], j,
                                            insert_barrier(i, j)));
            }
        }

        const double post_time = t.elapsed();

        std::cout <<
            "# queues: " << callbacks.size() <<
            ", tasks per queue: " << tasks_per_queue <<
            ", threads in pool: " << tp.getNumThreads() <<
            ", post duration (s): " << post_time <<
            std::endl;

        return post_time;
    }
};

TEST_F(ThreadPoolPerfTest, vanilla)
{
    nobarriers nb;
    run_test(nb,
             false,
             tasks_per_queue_,
             num_queues_,
             num_threads_,
             delay_us_,
             idle_queues_);
}

TEST_F(ThreadPoolPerfTest, vanilla_all_barriers)
{
    allbarriers ab;
    run_test(ab,
             false,
             tasks_per_queue_,
             num_queues_,
             num_threads_,
             delay_us_,
             idle_queues_);
}

TEST_F(ThreadPoolPerfTest, prefill)
{
    nobarriers nb;
    run_test(nb,
             true,
             tasks_per_queue_,
             num_queues_,
             num_threads_,
             delay_us_,
             idle_queues_);
}

TEST_F(ThreadPoolPerfTest, prefill_all_barriers)
{
    allbarriers ab;
    run_test(ab,
             true,
             tasks_per_queue_,
             num_queues_,
             num_threads_,
             delay_us_,
             idle_queues_);
}

TEST_F(ThreadPoolPerfTest, vanilla_queues_and_threads)
{
    nobarriers nb;
    run_tests(nb,
              false,
              tasks_per_queue_,
              num_queues_,
              num_threads_,
              delay_us_,
              idle_queues_);
}

TEST_F(ThreadPoolPerfTest, vanilla_queues_and_threads_barrier)
{
    allbarriers ab;
    run_tests(ab,
              false,
              tasks_per_queue_,
              num_queues_,
              num_threads_,
              delay_us_,
              idle_queues_);
}

TEST_F(ThreadPoolPerfTest, prefill_queues_and_threads)
{
    nobarriers nb;
    run_tests(nb,
              true,
              tasks_per_queue_,
              num_queues_,
              num_threads_,
              delay_us_,
              idle_queues_);
}

TEST_F(ThreadPoolPerfTest, prefill_queues_and_threads_barrier)
{
    allbarriers ab;
    run_tests(ab,
              true,
              tasks_per_queue_,
              num_queues_,
              num_threads_,
              delay_us_,
              idle_queues_);
}

}

// Local Variables: **
// mode: c++ **
// End: **
