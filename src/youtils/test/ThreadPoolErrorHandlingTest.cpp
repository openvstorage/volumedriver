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
#include "../TestBase.h"
#include "../ThreadPool.h"

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/utility.hpp>

namespace initialized_params
{

extern const char eh_threadpool_test_name[];
const char eh_threadpool_test_name[] = "test_threadpool";

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(eh_threadpool_test_threads,
                                                  uint32_t);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(eh_threadpool_test_threads,
                                      eh_threadpool_test_name,
                                      "eh_threadpool_test_threads",
                                      "Number of threads",
                                      ShowDocumentation::T,
                                      4);
}

namespace
{
struct ErrorHandlingTestPoolTraits
{
    static const bool
    requeue_before_first_barrier_on_error = true;

    static const bool
    may_reorder = true;

    typedef initialized_params::PARAMETER_TYPE(eh_threadpool_test_threads) number_of_threads_type;

    static const char* component_name;

    static const uint32_t max_number_of_threads = 32;

    static uint64_t
    sleep_microseconds_if_queue_is_inactive()
    {
        return 10000;
    }

    static uint64_t
    wait_microseconds_before_retry_after_error(uint32_t /* errorcount */)
    {
        return 10;
    }

    static int
    default_producer_id()
    {
        return 0;
    }
};

const char*
ErrorHandlingTestPoolTraits::component_name = "error_handling_thread_pool";

typedef youtils::ThreadPool<int, ErrorHandlingTestPoolTraits> ErrorHandlingTestPool;

template<typename T>
class ErrorHandlingTask
    : public ErrorHandlingTestPool::Task
{
public:
    ErrorHandlingTask(T& done,
                      const youtils::BarrierTask barrier,
                      uint32_t num_errors)
        : ErrorHandlingTestPool::Task(barrier)
        , done_(done)
        , num_errors_(num_errors)
    {}

    ErrorHandlingTask(const ErrorHandlingTask&) = delete;
    ErrorHandlingTask& operator=(const ErrorHandlingTask&) = delete;


    virtual const ErrorHandlingTestPool::Task::Producer_t&
    getProducerID() const
    {
        static const ErrorHandlingTestPool::Task::Producer_t id = 0;
        return id;
    }

    virtual void
    run(int /* thread_id */)
    {
        if (num_errors_-- > 0)
        {
            throw fungi::IOException("error");
        }

        done_(getErrorCount());
    }

    virtual const std::string&
    getName() const
    {
        static const std::string s("ErrorHandlingTask");
        return s;
    }

private:
    T& done_;
    uint32_t num_errors_;

};

}

namespace youtilstest
{
using namespace initialized_params;
using namespace youtils;

class ThreadPoolErrorHandlingTest
    : public TestBase
{
public:
    ThreadPoolErrorHandlingTest()
        : TestBase()
    {
        LOG_WARN("We might need to review this");
    }

protected:
    virtual void
    SetUp()
    {
        boost::property_tree::ptree pt;
        PARAMETER_TYPE(eh_threadpool_test_threads)(std::max<long>(sysconf(_SC_NPROCESSORS_ONLN), 2)).persist(pt);
        pt.put("version", 1);
        tp_.reset(new ErrorHandlingTestPool(pt));
    }

    virtual void
    TearDown()
    {
        if(tp_)
        {

            EXPECT_NO_THROW(tp_->stop()) << "Failed to stop threadpool";
            TestBase::TearDown();
        }
    }

    std::unique_ptr<ErrorHandlingTestPool> tp_;

    DECLARE_LOGGER("ThreadPoolErrorHandlingTest");
};

struct Callback
{
    Callback(boost::mutex& lock,
             boost::condition_variable& cond)
        : lock_(lock)
        , cond_(cond)
        , errors_(0)
        , done_(false)
    {}

    void
    operator()(uint32_t errors)
    {
        boost::unique_lock<boost::mutex> l(lock_);
        errors_ = errors;
        done_ = true;
        cond_.notify_one();
    }

    boost::mutex& lock_;
    boost::condition_variable& cond_;
    int errors_;
    bool done_;
};

TEST_F(ThreadPoolErrorHandlingTest, eins)
{
    boost::mutex lock;
    boost::condition_variable cond;

    Callback cb(lock, cond);

    const int num_errors = 10;
    ErrorHandlingTask<Callback>* t =
        new ErrorHandlingTask<Callback>(cb,
                                        BarrierTask::F,
                                        num_errors);

    boost::unique_lock<boost::mutex> l(lock);
    tp_->addTask(t);

    while (not cb.done_)
    {
        cond.wait(l);
    }

    EXPECT_EQ(num_errors, cb.errors_);
}

TEST_F(ThreadPoolErrorHandlingTest, zwei)
{
    boost::mutex lock;
    boost::condition_variable cond;

    boost::ptr_vector<Callback> callbacks;
    const uint32_t num_errors = 3;

    boost::unique_lock<boost::mutex> l(lock);

    for (size_t i = 0; i < 10; ++i)
    {
        callbacks.push_back(new Callback(lock, cond));
        typedef ErrorHandlingTask<Callback> Task;
        Task* t = new Task(callbacks[i],
                           (i % 5) ? BarrierTask::F : BarrierTask::T,
                           num_errors);
        tp_->addTask(t);
    }

    bool done = false;

    do
    {
        done = true;
        for (size_t i = 0; i < callbacks.size(); ++i)
        {
            if (not callbacks[i].done_)
            {
                done = false;
            }
        }

        if (not done)
        {
            cond.wait(l);
        }
    }
    while (not done);

    uint32_t errors = 0;
    for (size_t i = 0; i < callbacks.size(); ++i)
    {
        errors += callbacks[i].errors_;
    }

    EXPECT_EQ(num_errors * callbacks.size(), errors);
}

}

// Local Variables: **
// mode: c++ **
// End: **
