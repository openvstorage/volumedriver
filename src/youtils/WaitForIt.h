// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef WAIT_FOR_IT_H
#define WAIT_FOR_IT_H

#include "ThreadPool.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

namespace youtils
{

template <typename thread_pool_t>
class WaitForIt
{
    typedef typename thread_pool_t::Producer_t producer_t;
    typedef typename thread_pool_t::Task task_t;

    class WaitForItTask : public task_t
    {
    public:
        WaitForItTask(const producer_t& t,
                      boost::mutex& mut,
                      boost::condition_variable& cond_var,
                      bool& ready,
                      const BarrierTask barrier)
            : task_t(barrier),
              mutex_(mut),
              condition_variable_(cond_var),
              ready_(ready),
              t_(t)
        {}

    private:
        virtual const std::string&
        getName() const
        {
            static const std::string name("WaitForIt");
            return name;
        }

        virtual const producer_t&
        getProducerID() const
        {
            return t_;
        }
        DECLARE_LOGGER("WaitForItTask");

        virtual void
        run(int /*threadID*/)
        {
            LOG_TRACE("Inside WaitForItTask");
            {
                boost::lock_guard<boost::mutex> lock(mutex_);
                LOG_TRACE("Inside WaitForItTask, unlocking");
                ready_ = true;
            }
            LOG_TRACE("Inside WaitForItTask, notifying");
            condition_variable_.notify_all();
        }

        boost::mutex& mutex_;
        boost::condition_variable& condition_variable_;
        bool& ready_;
        const producer_t t_;
    };



public:

    explicit WaitForIt(const producer_t& v,
                       thread_pool_t* tp,
                       const BarrierTask barrier = BarrierTask::T)
        : ready_(false)
    {
        tp->addTask(new WaitForItTask(v, mutex_, condition_variable_, ready_, barrier));
    }
    DECLARE_LOGGER("WaitForIt");

    void
    wait()
    {
        boost::unique_lock<boost::mutex> lock(mutex_);
        while(!ready_)
        {
            LOG_TRACE("Inside WaitForIt, wait");
            condition_variable_.wait(lock);
        }
        LOG_TRACE("Inside WaitForIt, exit");
    }


    WaitForIt(const WaitForIt&) = delete;
    WaitForIt&
    operator=(const WaitForIt&) = delete;

private:
    boost::mutex mutex_;
    boost::condition_variable condition_variable_;
    bool ready_;
};

}

#endif // WAIT_FOR_THIS_BACKEND_WRITE

// Local Variables: **
// mode: c++ **
// End: **
