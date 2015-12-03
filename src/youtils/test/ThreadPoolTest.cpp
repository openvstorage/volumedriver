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

#include "../ThreadPool.h"
#include "../InitializedParam.h"
#include "../IOException.h"
#include "../SourceOfUncertainty.h"
#include "../TestBase.h"
#include "../WaitForIt.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <list>

#include <semaphore.h>

namespace initialized_params
{

extern const char threadpool_test_name[];
const char threadpool_test_name[] = "test_threadpool";

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(threadpool_test_threads,
                                                  uint32_t);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(threadpool_test_threads,
                                      threadpool_test_name,
                                      "threadpool_test_threads",
                                      "Number of threads",
                                      ShowDocumentation::T,
                                      4);
}

namespace
{

struct ThreadPoolTestTraits
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

    typedef initialized_params::PARAMETER_TYPE(threadpool_test_threads) number_of_threads_type;

    static const uint32_t max_number_of_threads = 256;

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

    static int
    default_producer_id()
    {
        return 0;
    }
};

const char* ThreadPoolTestTraits::component_name =
    initialized_params::threadpool_test_name;
}

namespace youtilstest
{

using namespace youtils;
using namespace fungi;
using namespace initialized_params;

namespace
{

class ScopedThreadPool : public ThreadPool<int,
                                           ThreadPoolTestTraits>
{
public:
    ScopedThreadPool(int num_treads = 2)
        : ThreadPool<int, ThreadPoolTestTraits>(fill_ptree(num_treads))
    {}

    ~ScopedThreadPool()
    {
        try
        {
            stop();
        }
        catch(...)
        {
            ADD_FAILURE() << "Could not stop threadpool, exception thrown";
        }
    }

    boost::property_tree::ptree
    fill_ptree(int num_threads)
    {

        PARAMETER_TYPE(threadpool_test_threads) parameter(num_threads);
        boost::property_tree::ptree pt;
        parameter.persist(pt);
        pt.put("version", 1);
        return pt;
    }
};


class Semaphore
{
public:
    Semaphore(unsigned int val = 0)
    {
        sem_ = new sem_t;

        if(-1 == sem_init(sem_,
                          0,
                          val))
        {
            throw fungi::IOException("Error creating semaphore", "", errno);
        }
    }

    ~Semaphore()
    {
        sem_destroy(sem_);
        delete sem_;
    }

    void
    post()
    {
        if(-1 == sem_post(sem_))
        {
            throw fungi::IOException("Error creating semaphore", "", errno);
        }
    }

    void
    wait()
    {
        if(-1 == sem_wait(sem_))
        {
            throw fungi::IOException("Error waiting on semaphore", "", errno);
        }
    }

    bool
    try_wait()
    {
        if(-1 == sem_trywait(sem_))
        {
            if (errno == EAGAIN)
            {
                return false;
            }
            else
            {
                throw fungi::IOException("Error waiting on semaphore", "", errno);
            }
        }
        return true;
    }


private:
    sem_t* sem_;
    Semaphore&
    operator=(const Semaphore& inOther);}
;


typedef ScopedThreadPool ThisThreadPoolType;

class IncrementTask : public ThisThreadPoolType::Task
{
public:
    IncrementTask(std::atomic<int>& i,
                  uint64_t sleep = 0 )
        : Task(BarrierTask::F)
        , i_(i)
        , sleep_(sleep)
    {}

    virtual void
    run(int)
    {
        usleep(sleep_ * 10000);
        ++i_;
    }

    virtual const std::string&
    getName() const
    {
        static std::string str("");
        return str;
    }

    virtual const int&
    getProducerID() const
    {
        static const int id = 0;
        return id;
    }

private:
    std::atomic<int>& i_;
    const uint64_t sleep_;
};

typedef WaitForIt<ThisThreadPoolType> WaitForItThisThreadPool;


class Test1Task : public ThisThreadPoolType::Task
{
public:
    Test1Task(bool& check,
              bool& get,
              const int id,
              Semaphore& sem,
              const BarrierTask barrier = BarrierTask::F)
        : Task(barrier),
          check_(check),
          get_(get),
          id_(id),
          sem_(sem)
    {}

    void
    run(int)
    {
        startorder.push_back(id_);
        sem_.post();

        while (not check_)
        {
            usleep(1000);
        }

        endorder.push_back(id_);
        get_ = true;
    }

    static std::vector<int> startorder;
    static std::vector<int> endorder;

    virtual const int&
    getProducerID() const
    {
        const static int id = 0;
        return id;
    }

    virtual const std::string&
    getName() const
    {
        static std::string str("");
        return str;
    }



protected:
    bool& check_;
    bool& get_;
    const int id_;
    Semaphore& sem_;

};

std::vector<int>
Test1Task::startorder;



std::vector<int>
Test1Task::endorder;


class Test2Task : public ThisThreadPoolType::Task
{
public:
    Test2Task(const int id,
              const BarrierTask barrier = BarrierTask::F)
        : Task(barrier),
          check_(false),
          id_(id)
    {}

    void
    run(int)
    {
        {
            ScopedSpinLock l(sorderLock);
            sorder.push_back(id_);
        }

        while (not check_)
        {
            usleep(1000);
        }
        {
            ScopedSpinLock l(eorderLock);
            eorder.push_back(id_);
        }

    }

    void
    go()
    {
        check_ = true;
    }

    static bool
    find_in_eorder(int id)
    {
        ScopedSpinLock l(eorderLock);
        return std::find(Test2Task::eorder.begin(),
                         Test2Task::eorder.end(),
                         id) != Test2Task::eorder.end();

    }

    static std::vector<int> sorder;
    static std::vector<int> eorder;
    static SpinLock sorderLock;
    static SpinLock eorderLock;

    const int&
    getProducerID() const
    {
        const static int id = 0;
        return id;
    }

    virtual const std::string&
    getName() const
    {
        static std::string str("");
        return str;
    }

    int
    getID()
    {
        return id_;
    }

protected:
    bool check_;
    const int id_;
};

std::vector<int>
Test2Task::sorder;
SpinLock Test2Task::sorderLock;
std::vector<int>
Test2Task::eorder;
SpinLock Test2Task::eorderLock;

class TestTask
    : public ThisThreadPoolType::Task
{
public:
    explicit TestTask(int i,
                      int sleep = 0,
                      int producer = 0,
                      const BarrierTask barrier = BarrierTask::F)
        : Task(barrier),
          i_(i),
          producer_(producer),
          sleep_(sleep)
    {
        std::stringstream ss;
        ss << "TestTask[" << i_ << "]";
        name_ = ss.str();
    }

    virtual void
    run(int threadid)
    {
        if (sleep_ > 0)
        {
            sleep(sleep_);
        }
        boost::lock_guard<boost::mutex> l(*TestTask::m);
        ran.push_back(i_);
        if (by.size() < 1000)
        {
            by.resize(1000);
        }
        ++(by[threadid]);
    }

    virtual const
    std::string & getName() const
    {
        return name_;
    }

    std::string name_;
    const int i_;
    const int producer_;

    const int sleep_;
    static std::list<int> ran;
    static std::vector<int> by;
    static boost::mutex *m;

    static bool hasRan(int i)
    {
        for (std::list<int>::const_iterator j = ran.begin();
             j != ran.end();
             ++j)
        {
            if (*j == i)
                return true;
        }
        return false;
    }

    static int
    getBy(int i)
    {
        return by[i];
    }

    virtual const int&
    getProducerID() const
    {
        return producer_;
    }

};

std::list<int> TestTask::ran;
std::vector<int> TestTask::by;
boost::mutex *TestTask::m;

class BlockingTask : public ThisThreadPoolType::Task
{
public:
    BlockingTask(boost::mutex& mutex,
                 int producer,
                 const BarrierTask barrier = BarrierTask::F)
        : Task(barrier),
          mutex_(mutex),
          producer_(producer)
    {}

    virtual void
    run(int /*threadid*/)
    {
        mutex_.lock();
        mutex_.unlock();
    }

    virtual const std::string&
    getName() const
    {
        static const std::string s( "BlockingTask");
        return s;
    }


    virtual const int&
    getProducerID() const
    {
        return producer_;
    }
private:
    boost::mutex& mutex_;
    int producer_;
};

class ThrowingTask
    : public ThisThreadPoolType::Task
{
public:
    ThrowingTask(int producer,
                 int id)
        : Task(BarrierTask::F),
          id_(id),
          times(0),
          producer_(producer)
    {}

    DECLARE_LOGGER("ThrowingTask");

    virtual void
    run(int /*threadid*/)
    {
        LOG_INFO("ID = " << id_);

        if(times < 10)
        {
            LOG_INFO("Throwing, ID = " << id_);
            ++times;
            throw "Something";
        }
        else
        {
            sleep(3);
        }
    }

    virtual const std::string&
    getName() const
    {
        static const std::string s( "ThrowingTask");
        return s;
    }


    virtual const int&
    getProducerID() const
    {
        return producer_;
    }
private:
    int id_;

    int times;
    int producer_;
};

class ThrowingTask2
    : public ThisThreadPoolType::Task
{
public:
    ThrowingTask2(int producer,
                  std::vector<time_t>& tms)
        : Task(BarrierTask::F)
        , tms_(tms)
        , times(0)
        , producer_(producer)
    {
        m_.lock();
    }

    virtual void
    run(int /*threadid*/)
    {
        if(times >= 5)
        {
            m_.unlock();
            return;
        }
        else
        {
            ++times;
            tms_.push_back(time(0));
            throw "Blah";
        }
    }

    ~ThrowingTask2()
    {
        sleep(1);
    }

    void
    lock()
    {
        m_.lock();
        m_.unlock();
    }

    virtual const std::string&
    getName() const
    {
        static const std::string s("ThrowingTask2");
        return s;
    }

    virtual const int&
    getProducerID() const
    {
        return producer_;
    }

private:
    boost::mutex m_;
    std::vector<time_t>& tms_;
    int times;
    int producer_;
};
}

class TestThreadPool
    : public TestBase
{
protected:
    virtual void
    SetUp()
    {
        TestTask::m = new boost::mutex();
        TestTask::ran.clear();
        TestTask::by.clear();
    }

    virtual void
    TearDown()
    {
        delete TestTask::m;
        TestTask::m = 0;
    }
};

class WaitForItTask
    : public ThisThreadPoolType::Task
{
public:
    WaitForItTask(const BarrierTask barrier = BarrierTask::F)
        : Task(barrier)
    {}

    void
    run(int)
    {
        usleep(1000);
    }

    virtual const int&
    getProducerID() const
    {
        const static int id = 0;
        return id;
    }

    virtual const std::string&
    getName() const
    {
        static std::string str("");
        return str;
    }
};


TEST_F(TestThreadPool, wait_for_it_the_first)
{
    youtils::SourceOfUncertainty sou;

    ThisThreadPoolType tp(sou(1,4));

    const int num_tasks = sou(1,15);
    for(int i = 0; i < num_tasks; ++i)
    {
        tp.addTask(new WaitForItTask());
    }

    WaitForItThisThreadPool wait(0,&tp);
    wait.wait();
    EXPECT_EQ(tp.getNumberOfTasks(0), (size_t)0);
    tp.stop(0);
}

TEST_F(TestThreadPool, wait_for_it)
{
    youtils::SourceOfUncertainty sou;

    const int num_threads = sou(1,10);
    ThisThreadPoolType tp(num_threads);
    std::atomic<int> counter(0);

    for(int i = 0; i <10000; ++i)
    {
        if(sou(1,100) < 5)
        {
            WaitForItThisThreadPool wait(0, &tp);
            wait.wait();
            EXPECT_TRUE(i == counter);
        }

        tp.addTask(new IncrementTask(counter));
    }
    WaitForItThisThreadPool wait(0, &tp);
    wait.wait();
    EXPECT_EQ(tp.getNumberOfTasks(0), (size_t)0);
    tp.stop(0);
}

TEST_F(TestThreadPool, wait_for_it_some_more)
{
    youtils::SourceOfUncertainty sou;

    const int num_threads = sou(1,10);

    ThisThreadPoolType tp(num_threads);
    std::atomic<int> counter(0);

    for(int i = 0; i < 100; ++i)
    {
        if(i > num_threads and
           sou(100) < 10)
        {
            WaitForItThisThreadPool wait(0, &tp, BarrierTask::F);
            int j = counter;
            wait.wait();
            EXPECT_TRUE(i == counter + num_threads - 1) << "i: " << i << ", counter: " << j << ", num_threads: " << num_threads;
        }

        tp.addTask(new IncrementTask(counter, i));
    }

    WaitForItThisThreadPool wait(0, &tp);
    wait.wait();
    EXPECT_EQ(tp.getNumberOfTasks(0), (size_t)0);
    tp.stop(0);
}

TEST_F(TestThreadPool, one)
{
    ThisThreadPoolType tp;
}

TEST_F(TestThreadPool, stop)
{
    ThisThreadPoolType tp;
    ASSERT_NO_THROW(tp.stop(0));
}

TEST_F(TestThreadPool, stop2)
{
    ThisThreadPoolType tp;
    for(int i = 0; i < 100; i++)
    {
        tp.addTask(new TestTask(0,2));
    }
    ASSERT_NO_THROW(tp.stop(0));
    ASSERT_EQ(tp.getNumberOfTasks(0), (size_t)0);
}
namespace
{
DECLARE_LOGGER("TestThreadPool");
}

TEST_F(TestThreadPool, stop3)
{
    ThisThreadPoolType tp;
    const int cnt = 10;

    LOG_INFO("Scheduling tasks that will throw");


    for(int i = 0; i < cnt; i++)
    {
        tp.addTask(new ThrowingTask(0,i));
    }

    ASSERT_NO_THROW(tp.stop(0));
    ASSERT_EQ(tp.getNumberOfTasks(0),(size_t)0);
    LOG_INFO("Stop requested, no more tasks left");
}

TEST_F(TestThreadPool, stop4)
{
    boost::mutex m;
    m.lock();
    ThisThreadPoolType tp(4);
    for(int i = 0; i < 4; i++)
    {
        tp.addTask(new BlockingTask(m,0));
    }
    while(not tp.tasksRunning(0))
    {
        sleep(1);
    }

    ASSERT_THROW(tp.stop(0,3),fungi::IOException);
    m.unlock();
    ASSERT_NO_THROW(tp.stop(0));
}

TEST_F(TestThreadPool, somejobs)
{
    ThisThreadPoolType tp;
    tp.addTask(new TestTask(0));
    tp.addTask(new TestTask(1));
    sleep(1);
    EXPECT_TRUE(TestTask::hasRan(0));
    EXPECT_TRUE(TestTask::hasRan(1));
}

TEST_F(TestThreadPool, hunderd)
{
    ThisThreadPoolType tp( 100);
    for (int i=0; i < 200; ++i)
    {
        tp.addTask(new TestTask(i));
    }
    sleep(1);
    for (int i=0; i < 200; ++i)
    {
        EXPECT_TRUE(TestTask::hasRan(i));
    }
    int count = 0;
    for (int i=0; i < 100; ++i)
    {
        count += TestTask::getBy(i);
    }
    EXPECT_EQ(200, count);
}

TEST_F(TestThreadPool, multiqueue)
{
    ThisThreadPoolType tp(10);
    boost::mutex m;
    m.lock();

    // put in 10 tasks to block the threads under id 1000
    for(int i =0; i < 10;i++)
    {
        tp.addTask(new BlockingTask(m,1000+i));
    }
    sleep(1);

    EXPECT_EQ((size_t)10,tp.getNumberOfQueues());
    for(int i =0; i < 10; i++)
    {
        EXPECT_EQ((size_t)0, tp.getNumberOfTasks(1000+i));
    }


    for(int i = 0; i < 100; ++i)
    {
        int j = i % 10;
        tp.addTask(new BlockingTask(m,j));
    }

    EXPECT_EQ((size_t)20,tp.getNumberOfQueues() );
    for(int i =0; i < 10; i++)
    {
        EXPECT_EQ((size_t)10,tp.getNumberOfTasks(i));
    }
    m.unlock();
    sleep(2);

    EXPECT_EQ((size_t)20,tp.getNumberOfQueues());
    for(int i =0; i < 10; i++)
    {
        EXPECT_EQ((size_t)0,tp.getNumberOfTasks(i));
    }
    for(int i =0; i < 10;i++)
    {
        tp.stop(i);
    }

    EXPECT_EQ((size_t)10,tp.getNumberOfQueues());
    for(int i =0; i < 10; i++)
    {
        tp.stop(1000+i);
    }

    EXPECT_EQ((size_t)0,tp.getNumberOfQueues());
}

namespace
{
int64_t microseconds;
}


TEST_F(TestThreadPool, OneThread)
{
    ThisThreadPoolType tp(1);
    youtils::wall_timer t;
    for(int i = 0; i < 20; ++i)
    {
        tp.addTask(new TestTask(i, 1,i%2));
    }
    retry:
    sleep(1);
    for(int i = 0; i < 20; ++i)
    {
        if(! TestTask::hasRan(i))
            goto retry;
    }


    microseconds = t.elapsed() * 1000 * 1000 ;
    LOG_INFO("Ran 20 tasks in 1 thread in : " << microseconds << " microseconds");

}

TEST_F(TestThreadPool, TwoThread)
{
    ThisThreadPoolType tp(2);
    youtils::wall_timer t;
    for(int i = 0; i < 20; ++i)
    {
        tp.addTask(new TestTask(i, 1,i));
    }
    retry:
    sleep(1);
    for(int i = 0; i < 20; ++i)
    {
        if(! TestTask::hasRan(i))
            goto retry;
    }

    double ms = t.elapsed() * 1000 * 1000;
    LOG_INFO("Ran 20 tasks in 2 threads in : " << ms << " microseconds");
    LOG_INFO("scaling: " << (microseconds / 2) << " microseconds");
}

TEST_F(TestThreadPool, ThreeThread)
{
    ThisThreadPoolType tp(3);
    youtils::wall_timer t;
    for(int i = 0; i < 20; ++i)
    {
        tp.addTask(new TestTask(i, 1,i));
    }
    retry:
    sleep(1);
    for(int i = 0; i < 20; ++i)
    {
        if(! TestTask::hasRan(i))
            goto retry;
    }

    LOG_INFO("Ran 20 tasks in 3 threads in : " << t.elapsed()* 1000 * 1000 << " microseconds");
    LOG_INFO( "scaling: " << (microseconds / 3) << " microseconds");
}

TEST_F(TestThreadPool, FourThread)
{
    ThisThreadPoolType tp(4);
    youtils::wall_timer t;
    for(int i = 0; i < 20; ++i)
    {
        tp.addTask(new TestTask(i, 1,i));
    }
    retry:
    sleep(1);
    for(int i = 0; i < 20; ++i)
    {
        if(! TestTask::hasRan(i))
            goto retry;
    }

    LOG_INFO("Ran 20 tasks in 4 threads in : " << t.elapsed() * 1000 * 1000 << " microseconds");
    LOG_INFO( "scaling: " << (microseconds / 4) << " microseconds");
}

TEST_F(TestThreadPool, fairness)
{
    ThisThreadPoolType tp( 10);
    boost::mutex m;
    m.lock();

    // put in 10 tasks to block the threads under id 1000
    for(int i =0; i < 10;i++)
    {
        tp.addTask(new BlockingTask(m,1000+i));
    }
    sleep(1);
    // 100 tasks in queue 0
    for(int i = 0; i < 100; ++i)
    {
        tp.addTask(new TestTask(i,1,0,BarrierTask::T));
    }
    // 10 tasks in queue 1
    for(int i = 100; i < 110; ++i)
    {
        tp.addTask(new TestTask(i,0,1));
    }
    m.unlock();
    sleep(3);
    EXPECT_EQ((size_t)0,tp.getNumberOfTasks(1));
    LOG_INFO( "Number of tasks in queue 0" << tp.getNumberOfTasks(0));

    EXPECT_TRUE(tp.getNumberOfTasks(0) > 0);
}

TEST_F(TestThreadPool, fairness2)
{
    ThisThreadPoolType tp( 10);
    boost::mutex m;
    m.lock();

    // put in 10 tasks to block the threads under id 1000
    for(int i =0; i < 10;i++)
    {
        tp.addTask(new BlockingTask(m,1000));
    }
    sleep(1);
    // 100 tasks in queue 0
    for(int i = 0; i < 100; ++i)
    {
        tp.addTask(new TestTask(i,1,0));
    }
    // 1 tasks in queue 1 - 10
    for(int i = 100; i < 110; ++i)
    {
        tp.addTask(new TestTask(i,1,i-99));
    }
    m.unlock();
    sleep(2);
    for(int i = 1; i< 10; ++i)
    {
        EXPECT_EQ((size_t)0,tp.getNumberOfTasks(i));
    }

    LOG_INFO("Number of tasks in queue 0" << tp.getNumberOfTasks(0));

    EXPECT_TRUE(tp.getNumberOfTasks(0) > 0);
}

TEST_F(TestThreadPool,fairness3)
{
    ThisThreadPoolType tp( 20);
    boost::mutex m;
    m.lock();

    // put in 10 tasks to block the threads under id 1000
    sleep(1);
    m.unlock();

    auto check([&](int i, int j)
               {
                   EXPECT_LT(abs(static_cast<int>(tp.getNumberOfTasks(i)) - static_cast<int>(tp.getNumberOfTasks(j))),
                             20);
               });

    for(int i = 0; i < 10000; i++)
    {
        tp.addTask(new TestTask(i,2,2));
        tp.addTask(new TestTask(i,1,0));
        tp.addTask(new TestTask(i,3,1));
        if(not i%30)
        {
            check(0, 1);
            check(0, 2);
            check(1, 2);
        }
    }

    sleep(20);

    check(0, 1);
    check(0, 2);
    check(1, 2);
}

TEST_F(TestThreadPool, barriertest1)
{
    bool ba1 = false, ba2 = false, ba3 = false, ba4 = false, ba5=true;

    Test1Task::startorder.clear();
    Test1Task::endorder.clear();
    Semaphore s1;
    Test1Task* t1 = new Test1Task(ba2,ba1,1,s1);
    Test1Task* t2 = new Test1Task(ba3,ba2,2,s1);
    Test1Task* t3 = new Test1Task(ba4,ba3,3,s1);
    Test1Task* t4 = new Test1Task(ba5,ba4,4,s1);
    ThisThreadPoolType tp(4);

    tp.addTask(t1);
    s1.wait();
    tp.addTask(t2);
    s1.wait();
    tp.addTask(t3);
    s1.wait();
    tp.addTask(t4);
    s1.wait();

    while (Test1Task::endorder.size() < 4)
    {
        sleep(1);
    }

    for(int i = 0; i < 4; ++i)
    {

        ASSERT_TRUE(Test1Task::startorder[i] == i+1);
        ASSERT_TRUE(Test1Task::endorder[i] == 4-i);
    }
}

TEST_F(TestThreadPool, barriertest2)
{
    bool ba1 = false, ba2 = false, ba3 = false, ba4 = false;
    Test1Task::startorder.clear();
    Test1Task::endorder.clear();

    Semaphore s1;
    Test1Task* t1 = new Test1Task(ba2,ba1,1,s1,BarrierTask::T);
    Test1Task* t2 = new Test1Task(ba3,ba2,2,s1,BarrierTask::T);
    Test1Task* t3 = new Test1Task(ba4,ba3,3,s1,BarrierTask::T);
    ThisThreadPoolType tp(4);
    tp.addTask(t1);
    s1.wait();
    tp.addTask(t2);
    tp.addTask(t3);


    ASSERT_TRUE(Test1Task::startorder.size() == 1);
    EXPECT_TRUE(Test1Task::startorder[0] == 1);
    ASSERT_TRUE(Test1Task::endorder.size() == 0);
    ASSERT_TRUE(s1.try_wait() == false);
    ba2 = true;
    s1.wait();

    ASSERT_TRUE(Test1Task::startorder.size() == 2);
    EXPECT_TRUE(Test1Task::startorder[1] == 2);
    ASSERT_TRUE(Test1Task::endorder.size() == 1);
    EXPECT_TRUE(Test1Task::endorder[0] == 1);
    ASSERT_TRUE(s1.try_wait() == false);
    ba3 = true;
    s1.wait();

    ASSERT_TRUE(Test1Task::startorder.size() == 3);
    EXPECT_TRUE(Test1Task::startorder[2] == 3);
    ASSERT_TRUE(Test1Task::endorder.size() == 2);
    EXPECT_TRUE(Test1Task::endorder[1] == 2);

    ba4 = true;
}

TEST_F(TestThreadPool, barriertest3)
{
    Test2Task::sorder.clear();
    Test2Task::eorder.clear();
    youtils::SourceOfUncertainty sou;

    std::vector<Test2Task*> tasks;
    const int max_test = 300;

    const unsigned cmod = sou(1, max_test);
    const unsigned num_tasks = sou(1, max_test);

    // std::cout << "XXX" << cmod << ", " << num_tasks << std::endl;

    for(unsigned i = 0; i < num_tasks; ++i)
    {
        tasks.push_back(new Test2Task(i, i % cmod == 0 ? BarrierTask::T : BarrierTask::F));
    }

    const int max_threads = 32;
    const unsigned num_threads = sou(1, max_threads);

    ThisThreadPoolType tp(num_threads);

    for(unsigned i = 0; i < num_tasks; ++i)
    {
        tp.addTask(tasks[i]);
    }

    for (auto task : tasks)
    {
        int id = task->getID();

        unsigned lower_barrier = id - (id % cmod);
        unsigned higher_barrier = lower_barrier + cmod;

        task->go();
        if((int)lower_barrier == id)
        {
            while(not Test2Task::find_in_eorder(lower_barrier))
            {
                usleep(1000);
            }
        }

        for(unsigned j = 0; j < lower_barrier; ++j)
        {
            if(not Test2Task::find_in_eorder(j))
            {
                FAIL() << "iteration " << id << " ,lower barrier" << lower_barrier << " ,failed to find " << j;
            }
        }

        for(unsigned j = higher_barrier; j < num_tasks; ++j)
        {
            if(Test2Task::find_in_eorder(j))
            {
                FAIL() << "iteration " << id << " ,lower barrier" << lower_barrier << " ,failed to find " << j;
            }
        }
    }
}

TEST_F(TestThreadPool,reschedule)
{
    ThisThreadPoolType tp( 10);
    boost::mutex m;
    m.lock();

    // put in 10 tasks to block the threads under id 1000
    for(int i =0; i < 10;i++)
    {
        tp.addTask(new BlockingTask(m,1000+i));
    }
    sleep(1);

    LOG_INFO("Scheduling tasks that will throw");

    for(int i = 0; i < 50; ++i)
    {
        tp.addTask(new ThrowingTask(i,i));
    }
    m.unlock();
    sleep(2);
    size_t tasks = 0;
    for(int i =0; i < 50;i++)
    {
        tasks += tp.getNumberOfTasks(i);
    }
    EXPECT_TRUE(tasks >= (size_t) 40) ;
}

TEST_F(TestThreadPool, exponentialBackoff)
{
    ThisThreadPoolType tp(10);
    boost::mutex m;
    m.lock();
    tp.addTask(new BlockingTask(m,1000));
    sleep(1);
    std::vector<time_t> tms;
    ThrowingTask2* t = new ThrowingTask2(0, tms);
    tp.addTask(t);
    m.unlock();
    t->lock();

    ASSERT_TRUE(tms.size() == 5);
    int time = 1;
    for(int i = 0; i < 4; i++)
    {
        time_t diff = difftime(tms[i+1], tms[i]);
        EXPECT_GE(diff, time);
        time *= 2;
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
