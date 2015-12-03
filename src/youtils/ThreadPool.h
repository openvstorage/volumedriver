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

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include "Assert.h"
#include "IOException.h"
#include "SpinLock.h"
#include "VolumeDriverComponent.h"
#include "InitializedParam.h"
#include "wall_timer.h"

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <memory>

#include <string>
#include <vector>

#include <boost/intrusive/list.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread.hpp>

#include "BooleanEnum.h"



namespace youtils
{
BOOLEAN_ENUM(BarrierTask);

template<typename T>
struct ThreadPoolTraits
{};

namespace bi = boost::intrusive;

template<typename T,
         typename traits = ThreadPoolTraits<T>>
class ThreadPool
    : public VolumeDriverComponent
{

#define LOCK_QUEUES()                                   \
    boost::lock_guard<lock_type> tqlg__(queues_lock_)

#define LOCK_MGMT()                                     \
    boost::lock_guard<lock_type> mguard__(mgmt_mutex_)

#define LOCK_CURRENT_TASKS()                            \
    fungi::ScopedSpinLock ctlg__(current_tasks_lock_)

private:
    class ThreadPoolRunnable
    {
    public:
        ThreadPoolRunnable(ThreadPool *tp, int threadId)
            : threadPool_(tp)
            , threadId_(threadId)
            , stop_(false)
        {}

        ThreadPoolRunnable(const ThreadPoolRunnable&) = delete;
        ThreadPoolRunnable& operator=(const ThreadPoolRunnable&) = delete;

        void stop()
        {
            stop_ = true;
        }

        void
        operator()()
        {
            while (!stop_)
            {
                try
                {
                    typename ThreadPool<T, traits>::Task* t =
                        threadPool_->getTask(this);
                    LOG_TRACE("[" << threadId_ << "] is running task: " <<
                              t->getName());
                    try
                    {
                        t->run(threadId_);
                        try
                        {
                            LOG_TRACE("[" << threadId_ << "] has finished task: " <<
                                      t->getName());
                            threadPool_->taskOk(t,this);
                        }

                        catch (fungi::IOException& e)
                        {
                            LOG_ERROR("IOException:" << e.what());
                        }
                    }
                    catch (std::exception &e)
                    {
                        LOG_ERROR("[" << threadId_ << "] task: " << t->getName() <<
                                  " failed:" << e.what());

                        try
                        {
                            LOG_TRACE("[" << threadId_ <<
                                      "] will reschedule task: " << t->getName());
                            threadPool_->taskNok(t, this);

                        }
                        catch (fungi::IOException& e)
                        {
                            LOG_ERROR("IOException:" << e.what());
                        }
                    }
                    catch (...)
                    {
                        LOG_ERROR("[" << threadId_ << "] task: " << t->getName() <<
                                  " failed with an unknown Exception");
                        try
                        {
                            threadPool_->taskNok(t,this);
                            LOG_TRACE("[" << threadId_ <<
                                      "] will reschedule task: " << t->getName());
                        }
                        catch (fungi::IOException& e)
                        {
                            LOG_ERROR("IOException:" << e.what());
                        }
                    }
                }
                catch (...)
                {
                    LOG_ERROR("[" << threadId_ << "] failed to get Task");
                }
            }
        }

        int
        id() const
        {
            return threadId_;
        }

    private:
        ThreadPool<T, traits> *threadPool_;
        int threadId_;
        bool stop_;
    };

public:
    typedef T Producer_t;
    class Task
        : public bi::list_base_hook<bi::link_mode<bi::auto_unlink> >
    {
    public:
        Task(const BarrierTask barrier)
            : barrier_(barrier)
            , errors_(0)
        {}

        Task(const Task&) = delete;
        Task&
        operator=(const Task&) = delete;

        virtual ~Task()
        {}

        virtual void
        run(int threadid) = 0;

        virtual const std::string &
        getName() const = 0;

        virtual const T&
        getProducerID() const = 0;

        typedef T Producer_t;

        BarrierTask
        barrier() const
        {
            return barrier_;
        }

        bool
        isBarrier() const
        {
            return barrier_ == BarrierTask::T;
        }

        uint32_t
        getErrorCount() const
        {
            return errors_;
        }

        void
        bumpErrorCount()
        {
            error_timer_.restart();
            ++errors_;
        }

        bool
        runnable()
        {
            return (errors_ == 0 or
                    microSecondsSinceLastError_() >= traits::wait_microseconds_before_retry_after_error(errors_));
        }

    private:
        const BarrierTask barrier_;
        mutable volatile boost::uint32_t errors_;
        youtils::wall_timer error_timer_;

        uint64_t
        microSecondsSinceLastError_()
        {
            return error_timer_.elapsed() * 1000.0 * 1000.0;
        }
    };

    class NOPTask
        : public Task
    {
    public:
        NOPTask(const T& consumerID)
            : Task(BarrierTask::F)
            , consumerID_(consumerID)
        {}

        virtual ~NOPTask()
        {}

        virtual void
        run(int threadId) override final
        {
            LOG_DEBUG(threadId);
        }

        virtual const std::string&
        getName() const override final
        {
            static std::string s("NOPTask");
            return s;
        }

        virtual const T&
        getProducerID() const override final
        {
            return consumerID_;
        }

    private:
        DECLARE_LOGGER("NOPTask");

        const T consumerID_;
    };

    bool
    update_for_test(const boost::property_tree::ptree& pt)
    {
        ConfigurationReport c_rep;
        UpdateReport u_rep;
        if(not checkConfig(pt, c_rep))
        {
            return false;
        }
        else
        {
            update(pt,
                   u_rep);
            return true;
        }
    }

private:
    // VolumeDriverComponent Interface
    virtual const char*
    componentName() const override final
    {
        return traits::component_name;
    }

    virtual void
    update(const boost::property_tree::ptree& pt,
           UpdateReport& rep) override final
    {
        LOCK_MGMT();

        if(not stop_)
        {
            typename traits::number_of_threads_type new_(pt);
            typename traits::number_of_threads_type::ValueType old_value =
                    num_threads.value();

            num_threads.update(pt,rep);
            if(new_.value() != old_value)
            {
                try
                {
                    {
                        LOCK_QUEUES();
                        LOCK_CURRENT_TASKS();
                        LOG_DEBUG("stopping runnables");

                        for (size_t i = 0; i < runnables_.size(); ++i)
                        {
                            runnables_[i].stop();
                        }
                    }

                    for (size_t i = 0; i < threads_.size(); i++)
                    {
                        T t = traits::default_producer_id();
                        addTask(new typename ThreadPool::NOPTask(t));
                    }
                        LOG_DEBUG("waiting for threads to finish");

                        for (size_t i = 0; i < threads_.size(); ++i)
                        {
                            threads_[i].join();
                        }
                        runnables_.clear();
                        currentTasks_.resize(num_threads.value());
                        init_();
                }
                catch(std::exception& e)
                {
                    LOG_WARN("Changing number of threads failed " << e.what());
                }
                catch(...)
                {
                    LOG_WARN("Changing number of threads failed ");
                }
            }
        }
        else
        {
            LOG_WARN("Threadpool was stopped, no resetting of threads happened");
        }
    }

    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault reportDefault = ReportDefault::F) const override final
    {
        num_threads.persist(pt,
                            reportDefault);
    }

    virtual bool
    checkConfig(const boost::property_tree::ptree& pt,
                ConfigurationReport& conf_rep) const override final
    {
        // TODO Y42 remove optional crud
        typename traits::number_of_threads_type val(pt);
        if(val.value() > traits::max_number_of_threads)
        {
            std::stringstream ss;
            ss << "Maximum number of threads is " << traits::max_number_of_threads
                    << ", value given " << val.value();
            ConfigurationProblem problem(componentName(),
                                         traits::number_of_threads_type::name(),
                                         ss.str());
            conf_rep.push_back(problem);
            return false;
        }
        return true;
    };

    typedef bi::list<Task,
                     bi::constant_time_size<false> > TaskQueue;

    class Queue
        : public TaskQueue
    {
    public:
        Queue(ThreadPool* p)
            : p_(p)
            , w_(0)
            , halted_(false)
        {}

        Queue(const Queue&) = delete;
        Queue& operator=(const Queue&) = delete;

        typedef TaskQueue this_type;

        bool
        getW()
        {
            return w_ > 0;
        }

        void
        addW()
        {
            ++w_;
        }

        void
        removeW()
        {
            --w_;
        }

        bool
        active()
        {
            if (halted_)
            {
                return false;
            }

            if (this_type::empty())
            {
                return false;
            }

            Task* t = &this_type::front();

            if (t->isBarrier() and w_ != 0)
            {
                return false;
            }

            if (traits::may_reorder)
            {
                for (typename QueueType::iterator it = this_type::begin();
                     it != this_type::end();
                     ++it)
                {
                    t = &(*it);

                    if (t->isBarrier() and it != this_type::begin())
                    {
                        return false;
                    }
                    else if (t->runnable())
                    {
                        if (it != this_type::begin())
                        {
                            this_type::erase(it);
                            this_type::push_front(*t);
                        }
                        return true;
                    }
                }

                return false;
            }
            else
            {
                return t->runnable();
            }
        }

        bool
        gethalted()
        {
            return halted_;
        }

        void
        setHalted(bool halted)
        {
            halted_ = halted;
        }

    private:
        ThreadPool* p_;
        uint32_t w_; // # of tasks being processed
        bool halted_;
    };

public:
    typedef Queue QueueType;
    typedef QueueType* QueueTypePtr;
    typedef std::map<T, QueueTypePtr> MapType_t;
    typedef typename MapType_t::iterator QueueIterator_t;

    explicit ThreadPool(const boost::property_tree::ptree& pt,
                        const RegisterComponent registerizle = RegisterComponent::T)
        : VolumeDriverComponent(registerizle,
                                pt)
        , num_threads(pt)
        , stop_(false)
        , queueIt_(taskQueues_.begin())
        , currentTasks_(num_threads.value())
    {
        try
        {
            LOCK_QUEUES();
            LOCK_CURRENT_TASKS();
            init_();
        }
        catch (...)
        {
            stop();
            throw;
        }
    }

    void
    init_()
    {
        queueIt_ = taskQueues_.begin();
        VERIFY(runnables_.size() == 0);

        for (size_t i = 0; i < num_threads.value(); ++i)
        {
            runnables_.push_back(new ThreadPoolRunnable(this, i));
            currentTasks_[i] = 0;
            threads_.push_back(new boost::thread(boost::ref(runnables_[i])));
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool()
    {
        if (!stop_)
        {
            try
            {
                stop();
            }
            catch(std::exception& e)
            {
                LOG_WARN("Exception during stopping of the threadpool: " << e.what());
            }
            catch(...)
            {
                LOG_WARN("Unknown exception during stopping of the threadpool. ");
            }
        }
    }

    void
    addTask(std::unique_ptr<ThreadPool::Task>&& t)
    {
        // Not 100% clear what to do here:
        // might throw after taking ownership, might throw before.
        addTask(t.release());
    }


    void
    addTask(ThreadPool::Task *t)
    {
        LOCK_QUEUES();

        if (stop_)
        {
            throw fungi::IOException("Can't add task: ThreadPool is stopping");
        }

        QueueIterator_t it = taskQueues_.find(t->getProducerID());
        if(it == taskQueues_.end())
        {
            QueueTypePtr theNewQueue = new QueueType(this);
            theNewQueue->push_back(*t);
            taskQueues_[t->getProducerID()] = theNewQueue;
            queueIt_ = taskQueues_.begin();
        }
        else
        {
            it->second->push_back(*t);
        }
        LOG_TRACE("Scheduled task " << t->getName());

        queues_cond_var_.notify_one();
    }

    Task *
    getTaskInt(ThreadPoolRunnable* tpr)
    {
        boost::unique_lock<lock_type> l(queues_lock_);

        Task* t = 0;
        QueueIterator_t it = queueIt_;

        if (not taskQueues_.empty() and
            not it->second->active())
        {
            do
            {
                ++it;
                if (it == taskQueues_.end())
                {
                    it=taskQueues_.begin();
                }
            }
            while(it!=queueIt_ and
                  not it->second->active());
        }

        if(taskQueues_.empty() or
           not it->second->active())
        {
            LOG_TRACE("No task for the wicked, going to sleep");
            uint64_t usecs =
                traits::sleep_microseconds_if_queue_is_inactive();
            queues_cond_var_.timed_wait(l,
                    boost::posix_time::microseconds(usecs));
            LOG_TRACE("Waking up and returning to work");
            return 0;
        }
        else
        {
            ++queueIt_;
            if(queueIt_ == taskQueues_.end())
            {
                queueIt_ = taskQueues_.begin();
            }
            t = &it->second->front();
            it->second->pop_front();
            it->second->addW();
            LOG_TRACE("Returning task " << t->getName());

            LOCK_CURRENT_TASKS();
            currentTasks_[tpr->id()] = t;
            return t;
        }
    }

    Task*
    getTask(ThreadPoolRunnable* tpr)
    {
        Task* t =0;
        while(!t)
        {
            t = getTaskInt(tpr);
        }
        LOG_TRACE("Got task " << t->getName() << " address " << t <<
                  " for ThreadPoolRunnable " << tpr);

        return t;
    }

    void
    halt(const T& id,
         unsigned int timeout = 1000)
    {
        {
            LOCK_QUEUES();
            QueueIterator_t it = taskQueues_.find(id);
            if (it == taskQueues_.end())
            {
                throw fungi::IOException("No such queue");
            }
            it->second->setHalted(true);
        }

        while(tasksRunning(id))
        {
            sleep(1);
            if(timeout-- == 0)
            {
                throw fungi::IOException("Could not stop queue " );
            }
        }
    }

    // Y42 misnomer, just clears the tasks from the volume...
    void
    stop(const T& id,
         unsigned int timeout = 1000)
    {
        {
            LOCK_QUEUES();
            QueueIterator_t it = taskQueues_.find(id);
            if(it != taskQueues_.end())
            {
                while(not it->second->empty())
                {
                    Task *t = &it->second->front();
                    it->second->pop_front();
                    delete t;
                }
                delete it->second;
                taskQueues_.erase(id);
                queueIt_ = taskQueues_.begin();
            }
        }
        while(tasksRunning(id))
        {
            sleep(1);
            if(timeout-- == 0)
            {
                throw fungi::IOException("Could not stop queue " );
            }
        }
    }

    void
    stop()
    {
        LOCK_MGMT();

        {
            LOCK_QUEUES();
            LOCK_CURRENT_TASKS();

            LOG_DEBUG("stopping runnables");

            for (size_t i = 0; i < runnables_.size(); ++i)
            {
                runnables_[i].stop();
            }
        }

        LOG_DEBUG("scheduling NOP tasks");

        for (size_t i = 0; i < threads_.size(); i++)
        {
            T t = traits::default_producer_id();
            addTask(new typename ThreadPool::NOPTask(t));
        }

        LOG_DEBUG("waiting for threads to finish");

        for (size_t i = 0; i < threads_.size(); ++i)
        {
            threads_[i].join();
        }

        {
            LOCK_QUEUES();
            while(taskQueues_.begin() != taskQueues_.end())
            {
                QueueIterator_t it = taskQueues_.begin();
                while(not it->second->empty())
                {
                    Task *t = &it->second->front();
                    it->second->pop_front();
                    delete t;
                }
                delete it->second;
                taskQueues_.erase(it->first);
                queueIt_ = taskQueues_.end();
            }
        }
        stop_ = true;
    }

    size_t
    getNumberOfQueues()
    {
        LOCK_QUEUES();
        return taskQueues_.size();
    }

    void
    getTaskIDs(std::vector<T>& vec)
    {
        LOCK_QUEUES();

        for(QueueIterator_t it = taskQueues_.begin();
            it != taskQueues_.end();
            ++it)
        {
            vec.push_back(it->first);
        }
    }

    template <typename T2>
    void
    mapper(const T& queue,
           T2& oper)
    {
        LOCK_QUEUES();

        QueueIterator_t it = taskQueues_.find(queue);
        if(it == taskQueues_.end())
        {
            return;
        }
        QueueTypePtr q = it->second;

        for(typename QueueType::const_iterator it2 = q->begin();
            it2 != q->end();
            ++it2)
        {
            typedef typename T2::type T1;

            const T1* t = dynamic_cast<const T1*>(&(*it2));
            if(t)
            {
                (oper)(t);
            }
        }
    }

    size_t
    getNumberOfTasks(const T& queue)
    {
        LOCK_QUEUES();

        QueueIterator_t it = taskQueues_.find(queue);
        if(it == taskQueues_.end())
        {
            return 0;
        }
        return it->second->size();
    }

    bool
    noTasksPresent(const T& queue)
    {
        LOCK_QUEUES();

        QueueIterator_t it = taskQueues_.find(queue);
        if(it == taskQueues_.end())
        {
            // Non existent queue is always in sync
            return true;
        }
        if(it->second->size() > 0)
        {
            return false;
        }

        return not tasksRunning(queue);
    }

    bool
    tasksRunning(const T& id)
    {
        LOCK_CURRENT_TASKS();

        for (size_t i = 0; i < currentTasks_.size(); ++i)
        {
            if (currentTasks_[i] != 0 and
                currentTasks_[i]->getProducerID() == id)
            {
                return true;
            }
        }

        return false;
    }

    unsigned
    getNumThreads() const
    {
        return threads_.size();
    }

    void
    taskOk(Task* t,
           ThreadPoolRunnable* tpr)
    {
        LOCK_QUEUES();
        LOCK_CURRENT_TASKS();

        LOG_TRACE("Task ok: " << t->getName());
        currentTasks_[tpr->id()] = 0;
        QueueIterator_t it = taskQueues_.find(t->getProducerID());
        delete t;
        if(it == taskQueues_.end())
        {
            LOG_WARN("Queue Gone, was probably stopped");
            throw fungi::IOException("Queue gone");
        }
        it->second->removeW();
    }

    void
    taskNok(Task* t,
            ThreadPoolRunnable* tpr)
    {
        LOCK_QUEUES();
        LOCK_CURRENT_TASKS();

        LOG_TRACE("Task not ok: " << t->getName());
        t->bumpErrorCount();
        t->getErrorCount();
        currentTasks_[tpr->id()] = 0;
        QueueIterator_t it = taskQueues_.find(t->getProducerID());
        if(it == taskQueues_.end())
        {
            delete t;
            LOG_ERROR("Queue Gone");
            throw fungi::IOException("Queue gone");
        }

        Queue* q = it->second;

        if (not traits::requeue_before_first_barrier_on_error
            or t->isBarrier())
        {
            q->push_front(*t);
        }
        else
        {
            bool inserted = false;
            for (typename QueueType::iterator qit = q->begin();
                 qit != q->end();
                 ++qit)
            {
                if ((*qit).isBarrier())
                {
                    q->insert(qit, *t);
                    inserted = true;
                    break;
                }
            }

            if (!inserted)
            {
                q->push_back(*t);
            }
        }
        it->second->removeW();
    }

private:
    //    const std::string name_;
    typename traits::number_of_threads_type num_threads;

    static const unsigned scS = 1;

    boost::ptr_vector<boost::thread> threads_;
    boost::ptr_vector<ThreadPoolRunnable> runnables_;

    MapType_t taskQueues_;

    typedef boost::mutex lock_type;
    lock_type queues_lock_;
    boost::condition_variable queues_cond_var_;

    fungi::SpinLock current_tasks_lock_;
    lock_type mgmt_mutex_;

    bool stop_;

    //INVARIANT: (queueIt_ == taskQueues_.end()) => taskQueue.empty()
    QueueIterator_t queueIt_;
    std::vector<Task*> currentTasks_;

    DECLARE_LOGGER("ThreadPool");

#undef LOCK_QUEUES
#undef LOCK_CURRENT_TASKS
#undef LOCK_MGMT

};
}

#endif /* THREADPOOL_H_ */

// Local Variables: **
// mode: c++ **
// End: **
