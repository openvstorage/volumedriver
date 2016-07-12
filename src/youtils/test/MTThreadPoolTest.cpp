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
#include "../IOException.h"
#include "../SourceOfUncertainty.h"
#include "../SpinLock.h"
#include <gtest/gtest.h>
#include "../ThreadPool.h"

#include <algorithm>
#include <cerrno>
#include <list>

#include <semaphore.h>

namespace initialized_params
{
extern const char mt_threadpool_test_name[];
const char mt_threadpool_test_name[] = "test_threadpool";

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(mt_threadpool_test_threads,
                                                  uint32_t);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(mt_threadpool_test_threads,
                                      mt_threadpool_test_name,
                                      "mt_threadpool_test_threads",
                                      "Number of threads",
                                      ShowDocumentation::T,
                                      4);
}

namespace
{

struct MTThreadPoolTraits
{
    // always insert failed tasks at the head of the queue
    static const bool
    requeue_before_first_barrier_on_error = false;

    // tasks before a barrier cannot be reordered
    static const bool
    may_reorder = false;

    static uint64_t
    sleep_microseconds_if_queue_is_inactive()
    {
        return 1000000;
    }

    typedef initialized_params::PARAMETER_TYPE(mt_threadpool_test_threads) number_of_threads_type;

    static const uint32_t max_number_of_threads;

    static const char* component_name;

    // exponential backoff based on errorcount
    static uint64_t
    wait_microseconds_before_retry_after_error(uint32_t errorcount)
    {
        switch (errorcount)
        {
#define SECS * 1000000

        case 0: return 0 SECS;
        case 1: return 1 SECS;
        case 2: return 2 SECS;
        case 3: return 4 SECS;
        case 4: return 8 SECS;
        case 5: return 15 SECS;
        case 6: return 30 SECS;
        case 7: return 60 SECS;
        case 8 : return 120 SECS;
        case 9: return 240 SECS;
        default : return 300 SECS;

#undef SECS
        }
    }

    static uint32_t
    default_producer_id()
    {
        return 0;
    }
};

const uint32_t
MTThreadPoolTraits::max_number_of_threads = 256;

const char* MTThreadPoolTraits::component_name =
    initialized_params::mt_threadpool_test_name;
}

namespace youtilstest
{
using namespace youtils;
using namespace fungi;
using namespace initialized_params;

class MTThreadPoolTest
    : public testing::Test
{};

class BigThreadPool
    : public ThreadPool<uint32_t, MTThreadPoolTraits>
{
    typedef ThreadPool<uint32_t, MTThreadPoolTraits> ThreadPool_t;

public:
    BigThreadPool(const boost::property_tree::ptree pt)
        : ThreadPool_t(pt)
    {}
};

class NumberOfThreadsResetter
{
public:
    NumberOfThreadsResetter(BigThreadPool& tp,
                            uint32_t current_number_of_threads)
        : threadpool_(tp)
        , number_of_threads(current_number_of_threads)
        , stop_(false)
    {
    }

    void
    operator()()
    {
        while(not stop_)
        {
            sleep(1);
            number_of_threads  = sof(static_cast<uint32_t>(1),
                                     MTThreadPoolTraits::max_number_of_threads);
            std::cerr << "Resetting num threads to " << number_of_threads;

            boost::property_tree::ptree pt;
            PARAMETER_TYPE(mt_threadpool_test_threads)(number_of_threads).persist(pt);
            pt.put("version",1);

            if(number_of_threads > MTThreadPoolTraits::max_number_of_threads)
            {
                ASSERT_FALSE(threadpool_.update_for_test(pt));
            }
            else
            {
                ASSERT_TRUE(threadpool_.update_for_test(pt));
            }
            std::cerr << "finished";
        }
    }

    void
    stop()
    {
        stop_ = true;
    }

    BigThreadPool& threadpool_;
    uint32_t number_of_threads;
    bool stop_;
    youtils::SourceOfUncertainty sof;
};

class Receiver
{
public:
    Receiver(const uint32_t number_of_queues,
             const std::vector<uint32_t>& bars)
        : queues(number_of_queues)
        , intermediates(number_of_queues)
        , spinlocks(number_of_queues)
        , barriers(bars)
    {
        for(unsigned i = 0; i < number_of_queues; ++i)
        {
            spinlocks[i] = new  SpinLock();
        }

#ifndef NDEBUG
        for (const auto i : queues)
        {
            ASSERT(i == 0);
        }
#endif
    }

    ~Receiver()
    {
        for (auto sp : spinlocks)
        {
            delete sp;
        }
    }

    void
    operator()(const uint32_t queue_num,
               const uint64_t number,
               bool barrier)
    {
        ASSERT_TRUE(queue_num < queues.size());
        ScopedSpinLock ssl(*spinlocks[queue_num]);
        //std::cerr << "Queue num " << queue_num << " number " << number << ", barrier : " << barrier << std::endl;
        ASSERT_TRUE(number < queues[queue_num] + (2*barriers[queue_num]));
        ++intermediates[queue_num];
        if(number % 1024 == 0)
        {
            std::cerr << "Another Ki tasks for queue " << queue_num;

        }
        if(barrier)
        {
            queues[queue_num] += barriers[queue_num];
            ASSERT_TRUE(intermediates[queue_num] >= barriers[queue_num]);
            intermediates[queue_num] -= barriers[queue_num];
            ASSERT_TRUE(intermediates[queue_num] < barriers[queue_num]);
        }
    }

    std::vector<uint64_t> queues;
    std::vector<uint64_t> intermediates;
    std::vector<SpinLock*> spinlocks;
    const std::vector<uint32_t>& barriers;
};

class Task
    : public BigThreadPool::Task
{
public:
    Task(Receiver&r,
         const uint32_t queue_num,
         const uint64_t num,
         const BarrierTask barrier)
        : BigThreadPool::Task(barrier)
        , rec(r)
        , q_n(queue_num)
        , n(num)
    {}

    virtual void
    run(int)
    {
        rec(q_n,
            n,
            isBarrier());
    }

    virtual const std::string&
    getName() const
    {
        return name;
    }


    const std::string name;

    virtual const    uint32_t&
    getProducerID() const
    {
        return q_n;
    }

    Receiver& rec;
    uint32_t q_n;
    uint64_t n;
};


TEST_F(MTThreadPoolTest, DISABLED_torture)
{
    youtils::SourceOfUncertainty sou;

    const uint32_t num_queues = sou(1,16);
    std::vector<uint32_t> barriers(num_queues);

    for(unsigned i = 0; i < num_queues; ++i)
    {
        barriers[i] = sou(1, 128);
    }

    for (auto num : barriers)
    {
        ASSERT_TRUE(num > 0);
    }

    Receiver rec(num_queues,
                 barriers);
    std::cerr << "testing with " << num_queues << " queues";

    uint32_t number_of_threads  = sou(MTThreadPoolTraits::max_number_of_threads);

    std::cerr << "starting with " << number_of_threads << " threads";

    boost::property_tree::ptree pt;
    PARAMETER_TYPE(mt_threadpool_test_threads)(number_of_threads).persist(pt);
    pt.put("version",1);

    BigThreadPool btp(pt);
    NumberOfThreadsResetter resetter(btp,
                                    number_of_threads);

    boost::thread resetter_thread(boost::ref(resetter));

    for(uint64_t i = 0; i < 1024*1024; ++i)
    {
        for(uint64_t k = 0; k < num_queues; ++k)
        {
            btp.addTask(new Task(rec, k, i, (i%barriers[k] == (barriers[k]-1)) ? BarrierTask::T : BarrierTask::F));
        }
    }
    bool choices_choices = sou(0,1);
    if(choices_choices)
    {
        std::cerr << "Waiting till all threads are stopped";

        bool cont = true;

        while(cont)
        {
            sleep(5);

            for(uint32_t i = 0; i < num_queues; ++i)
            {
                if((cont = btp.getNumberOfTasks(i)))
                    break;
            }
        }
    }
    else
    {
        std::cerr << " Stopping the threadpool" << std::endl;
        btp.stop();
    }

    resetter.stop();
    resetter_thread.join();
}

}
