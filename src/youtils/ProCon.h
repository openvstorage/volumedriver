#ifndef _PRO_CON_H
#define _PRO_CON_H

//
// Copyright (c) 2002 by Ted T. Yuan.
//
// Permission is granted to use this code without restriction as long as this copyright notice appears in all source files.
//

#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/shared_ptr.hpp>
#include <deque>
#include <vector>
#include <iostream>

#ifndef SPACE_YIN      // either you define the space name
#define SPACE_YIN yin  // or I define it with a default name "yin"
#endif

// in most cases, you do not want to define HAS_PUTTABLETAKABLE,
// it forces you to define Takable, Puttable in your code
#ifdef HAS_PUTTABLETAKABLE
#define _HAS_PUTTABLETAKABLE
#endif

// namespace is 'yin', the yin classes are usually sub-classed
// by applications, the sub-classes that contain application logic
// are to be in the 'yang' namespace
namespace SPACE_YIN {

/////////////////////////////////////////
// forward declarations...
class Latch;
class Gate;

// sync'd implementation of the corresponding std container classes
template < typename _Tp, typename _queueTp >
    class Channel; // intend to be a linear queue, FIFO
template < typename _Tp > struct Pool;
template < typename _Tp >
    struct Channels; // a pool of Channel objects

// access syntax api of the above implementation,
// _ChannelTp can be Channel or Channels
// TODO - _ChannelTp should be defined in a trait class...
template < typename _Tp, typename _ChannelTp > struct Takable;
template < typename _Tp, typename _ChannelTp > struct Puttable;

// producer-consumer idiom implemetation templates
template < typename _Tp, typename _ChannelTp > class Producer;
template < typename _Tp, typename _ChannelTp > class Consumer;

// Production has producers and consumers which are connected through queue
template < typename _Tp, typename _ChannelTp,
    typename _ProducerTp, typename _ConsumerTp >
    class Production;

// Consuming is a pool of consumers to consume a static queue
template < typename _Tp, typename _ChannelTp,
    typename _ConsumerTp > class Consuming;



/////////////////////////////////////////
// typedefs and convenient functions
typedef boost::mutex Mutex;
typedef boost::condition Condition;
typedef boost::mutex::scoped_lock Lock;

inline static void sleep(
    int secs, int msecs = 0, int usecs = 0, int nsecs = 0)
{
    boost::xtime xt;
    boost::xtime_get(&xt, boost::TIME_UTC_);

    if(nsecs > 1000)
    {
        usecs += nsecs / 1000;
        nsecs = nsecs % 1000;
    }
    if(usecs > 1000)
    {
        msecs += usecs / 1000;
        usecs = usecs % 1000;
    }
    if(msecs > 1000)
    {
        secs += msecs / 1000;
        msecs = msecs % 1000;
    }

    xt.sec += secs;
    xt.nsec += (long)msecs * 1000000L + usecs * 1000L + nsecs;

    boost::thread::sleep(xt);
}

// if you want logging you need to define WITH_LOGGING,
// then call Logger::log("any text"); once in your main thread
// before using the classes in this file. It assures
// Logger::instance() to run properly (static Mutex s_mu is created un-interruptedly).


//todo [BDV] integrate logging to voldrv logging
#ifdef WITH_LOGGING
#define _WITH_LOGGING
#endif

// debug classes...
struct Logger
{
#ifdef _WITH_LOGGING
// call this once at start of application (main thread) to make sure s_mu is proper
inline static Mutex & instance()
{
    static Mutex s_mu;
    return(s_mu);
}
#endif

inline static void log(const char *
#ifdef _WITH_LOGGING
                       buf
#endif
)
{
#ifdef _WITH_LOGGING
    // can not protect if an outside thread that does not know the mutex and accesses std::cout directly
    Lock lock(instance());
    std::cout << buf << std::endl << std::flush;
#endif
}

inline static void beeper(int interval, char * msg)
{
    if(msg) {
       SPACE_YIN::sleep(interval);
       log(msg);
    }
}
};

class pc_exception : public std::runtime_error
{
public:
    explicit pc_exception(const std::string& msg)
        : std::runtime_error(msg) {}
};

/////////////////////////////////////////
// implementation


// _queueTp has to have Container traits
template < typename _Tp, typename _queueTp = std::deque<_Tp> >
class Channel : private _queueTp
{
private:
    size_t maxSize_;
    Mutex monitor_;
    Condition bufferNotFull_, bufferNotEmpty_;
    volatile bool bMayStop_;

public:
    explicit Channel(size_t limit = (size_t)-1) : maxSize_(limit < 1 ? 1 : limit), bMayStop_(false) {}

    Channel(_queueTp& queue)
        : _queueTp(queue), maxSize_(queue.max_size()) {}


    virtual ~Channel() {}

    // for consumer thread...
    _Tp poll(long /*msecs*/ = -1) // ignore msecs for now
    {
        Lock lk(monitor_);
        while (!bMayStop_ and 0 == ((_queueTp *)this)->size())
        {
            bufferNotEmpty_.wait(lk);
        }

        // outside caller intentionally calls for stop, last resort?
        if(bMayStop_ && 0 == ((_queueTp *)this)->size())
            throw pc_exception("consumer to end");

        // pop back
        _Tp item = pop();
        bufferNotFull_.notify_one();
        return item;
    }

    // for producer thread...
    bool offer(_Tp item, long /*msecs*/ = -1) // ignore msecs for now
    {
        Lock lk(monitor_);
        while (!bMayStop_ && maxSize_ == ((_queueTp *)this)->size())
        {
            bufferNotFull_.wait(lk);
        }

        // outside caller intentionally calls for stop
        if(bMayStop_)
            throw pc_exception("producer may end");

        // push front
        push(item);
        bufferNotEmpty_.notify_one();
        return true;
    }

    virtual void mayStop(bool bMayStop = true)
    {
        Lock lk(monitor_);
        bMayStop_ = bMayStop;
        if(bMayStop) // if outside says may stop, wake up the waiting threads...
        {
            bufferNotEmpty_.notify_all();
            bufferNotFull_.notify_all();
        }
    }

    // for outside callers only, calling this on Linux 7.2 within
    // the above poll() or offer() will deadlock
    typename _queueTp::size_type size()
    {
        Lock lk(monitor_);
        return ((_queueTp *)this)->size();
    }
protected:
    virtual _Tp pop() // retrieve from end
    {
        _Tp item = ((_queueTp *)this)->back();
        ((_queueTp *)this)->pop_back();
        return item;
    }

    virtual void push(const _Tp item) // insert at head...
    {
        ((_queueTp *)this)->insert(((_queueTp *)this)->begin(), item);
    }
};

// Pool provides thread-safe access to std::vector<_Tp>
template < typename _Tp >
struct Pool : public Channel< _Tp, std::vector<_Tp> > {
    Pool(size_t limit = (size_t)-1) : Channel< _Tp, std::vector<_Tp> >(limit) {}

    Pool(std::vector<_Tp>& queue)
        : Channel< _Tp, std::vector<_Tp> >(queue) {}
};

// Channels is intended to be a thread-safe vector of thread-safe queues...
template < typename _Tp >
struct Channels : public Pool< Channel<_Tp> >
{
    Channels(size_t size = (size_t)-1) : Pool< Channel<_Tp> >(size) {}

    _Tp poll(long msecs = -1) // ignore msecs for now
    {
        Channel<_Tp> channel = ((Pool< Channel<_Tp> >*)this)->poll(msecs);
        return channel.poll(msecs);
    }
    bool offer(_Tp item, long msecs = -1) // ignore msecs for now
    {
        Channel<_Tp> channel = poll(msecs);
        return channel.offer(item, msecs);
    }
};

// taker API on top of thread-safe _ChannelTp
template < typename _Tp, typename _ChannelTp = Channel<_Tp> >
struct Takable // thread-safe storage iterator
{
    _ChannelTp& channel_;
    Takable(_ChannelTp& chan) : channel_(chan) {}

    virtual ~Takable() {}

    virtual _Tp
    take(long msecs = -1)
    {
        try
        {
            return channel_.poll(msecs);
        }
        catch (pc_exception& pce)
        {
            throw;
        }
        catch (std::exception& e)
        {
            Logger::log(e.what());
            throw;
        }
        catch (...)
        {
            Logger::log("got an unknown exception");
            throw std::runtime_error("unknown exception");
        }
    }
};

// inserter API on top of thread-safe _ChannelTp
template < typename _Tp, typename _ChannelTp = Channel<_Tp> >
struct Puttable // thread-safe storage inserter
{
    _ChannelTp& channel_;
    Puttable(_ChannelTp& chan) : channel_(chan) {}

    virtual ~Puttable() {}

    virtual void put(_Tp item, long msecs = -1)
    {
        try
        {
            channel_.offer(item, msecs);
        }
        catch (std::exception& e)
        {
            Logger::log(e.what());
            throw;
        }
        catch (...)
        {
            Logger::log("got an unknown exception");
            throw std::runtime_error("unknown exception");
        }
    }
};

// Locking mechanisms
class Latch
{
protected:
    volatile bool state_;
#define latched_ state_
    Mutex monitor_;
    Condition cond_;
public:
    explicit Latch(bool state = false) : latched_(state) {}
    bool attempt(long /*msecs*/)
    {
        return false; // function holder, not implemented yet
    }
    void acquire()
    {
        Lock lk(monitor_);
        while (!latched_)
            cond_.wait(lk);
    }
    // enables all current and future acquires to pass
    void release(bool bRelease = true)
    {
        Lock lk(monitor_);
        latched_ = bRelease;
        cond_.notify_all();
    }
#undef latched_
};

// use object pointers of type _Tp to go around STL's value semantic...
template < typename _Tp, typename _ChannelTp = Channel<_Tp> >
class Producer
{
    Puttable<_Tp, _ChannelTp> * ptr_holder_;
    boost::shared_ptr< Puttable<_Tp, _ChannelTp> > holder_;
protected:
    Puttable<_Tp, _ChannelTp>& channel_;
    Latch& latch_;
    volatile bool bMayStop_;

public:
    Producer(Puttable<_Tp, _ChannelTp>& channel, Latch& lh)
        : channel_(channel), latch_(lh), bMayStop_(true) {}

    Producer(_ChannelTp& channel, Latch& lh)
        : ptr_holder_( new Puttable<_Tp, _ChannelTp>(channel) ),
        holder_(ptr_holder_), channel_(*holder_),
        latch_(lh), bMayStop_(true) {}

    virtual ~Producer() {}

    void operator()()
    {
      latch_.acquire();
      started();

      // bMayStop_ is a suggest to stop from outside, overwritable cancel() is internal condition...
      for(;!(bMayStop_ && cancel());)
      {
          try
          {
              _Tp o = produce();
              channel_.put(o);
          }
          catch (pc_exception&)
          {
              break;
          }
          catch (std::exception& e)
          { // you threw
              Logger::log(e.what());
              break;
          }
          catch (...)
          { // you threw
            Logger::log("Caught unknown exception in Producer::produce");
            break;
          }

          boost::thread::yield(); // need breath...
      }

      done();
    }

    virtual void mayStop(bool bMayStop = true)
        { channel_.channel_.mayStop(bMayStop_ = bMayStop); }

protected: // overwritable by derived classes...
    virtual _Tp produce() = 0;
    virtual bool cancel() { return false; } // defualt implementation is never to cancel
    virtual void started() {
        Logger::log("producer started");
    }
    virtual void done() {
        Logger::log("producer done");
    }
};

template < typename _Tp, typename _ChannelTp = Channel<_Tp> >
class Consumer
{
    Takable<_Tp, _ChannelTp> * ptr_holder_;
    boost::shared_ptr< Takable<_Tp, _ChannelTp> > holder_;
protected:
    Takable<_Tp, _ChannelTp>& channel_;
    Latch& latch_;
    long checkEmptyWithinMSec_;
    volatile bool bMayStop_;

public:
    Consumer(Takable<_Tp, _ChannelTp>& channel, Latch& lh)
        : channel_(channel), checkEmptyWithinMSec_(-1),
        latch_(lh), bMayStop_(true) {}

    Consumer(_ChannelTp& channel, Latch& lh)
        : ptr_holder_( new Takable<_Tp, _ChannelTp>(channel) ),
        holder_(ptr_holder_), channel_(*holder_),
        latch_(lh), checkEmptyWithinMSec_(-1), bMayStop_(true) {}

    virtual ~Consumer() {}

    void operator()()
    {
      latch_.acquire();
      started();

      // bMayStop_ is a suggest to stop from outside, overwritable cancel() is internal condition...
      for(;!(bMayStop_ && cancel());)
      {
        try
        {
            // wait for within_msec milli-seconds to make sure no new tasks coming in...
            _Tp item = channel_.take(checkEmptyWithinMSec_ < 0 ? -1 : checkEmptyWithinMSec_);
            consume(item);
        }
        catch (pc_exception&)
        {
            // thrown by mayStop() in _ChannelTp
            break;
        }
        catch (std::exception& e)
        {
            // you threw
            Logger::log(e.what());
            break;
        }
        catch (...)
        {
            // you threw
            Logger::log("unknown exception in Consumer");
            break;
        }

        boost::thread::yield(); // need breath...
      }

      done();
    }

    virtual void mayStop(bool bMayStop = true)
        { channel_.channel_.mayStop(bMayStop_ = bMayStop); }

protected: // overwritable by derived classes...
    virtual void consume(_Tp /*x*/) {
       Logger::log("consumer not implemented");
    }
    virtual bool cancel() { return false; } // default implementation is never to cancel
    virtual void started() { // defualt implementation does nothing
        Logger::log("consumer started");
    }
    virtual void done() { // defualt implementation does nothing
        Logger::log("consumer done");
    }
};

// thread-safe, sustainable production-line logic that chains producers with consumers
template < typename _Tp,
    typename _ChannelTp = Channel<_Tp>,
    typename _ProducerTp = Producer<_Tp, _ChannelTp>,
    typename _ConsumerTp = Consumer<_Tp, _ChannelTp> >
class Production
{
private:
    size_t queueLen_, nProducers_, nConsumers_;
    bool syncStart_, bHidePuttableTakable_;
public:
    // bHideTakable indicates derived classes of Producer and Consumer
    // do not have custom implementation of Puttable and Takable interfaces
    explicit Production(size_t np = 1, size_t nc = 1,
        bool sc = true, size_t ql = (size_t)-1, bool bHideTakable = false)
        : nProducers_(np), nConsumers_(nc), syncStart_(sc),
        queueLen_(ql), bHidePuttableTakable_(bHideTakable) {}

    void operator()()
    {
        _ChannelTp chan(queueLen_);

        Latch theLatch, noLatch(true);

#if defined(_HAS_PUTTABLETAKABLE)
        Puttable<_Tp, _ChannelTp> puttable(chan);
        Takable<_Tp, _ChannelTp>  takable(chan);

        _ProducerTp* producer = bHidePuttableTakable_
            ? new _ProducerTp(chan, noLatch)
            : new _ProducerTp(puttable, noLatch) ;
        _ConsumerTp* consumer = bHidePuttableTakable_
            ? new _ConsumerTp(chan, syncStart_ ? theLatch : noLatch)
            : new _ConsumerTp(takable, syncStart_ ? theLatch : noLatch);
#else
        _ProducerTp* producer =
            new _ProducerTp(chan, noLatch);
        _ConsumerTp* consumer =
            new _ConsumerTp(chan, syncStart_ ? theLatch : noLatch);
#endif
        std::auto_ptr<_ProducerTp> prodClean(producer);
        std::auto_ptr<_ConsumerTp> consClean(consumer);
        consumer->mayStop(false);

        pcModelCreated(*producer, *consumer);

        try {
            boost::thread_group pthreads, cthreads;
            size_t i;
            for (i = 0; i < nProducers_; ++i)
                pthreads.create_thread(*producer);

            for (i = 0; i < nConsumers_; ++i)
                cthreads.create_thread(*consumer);

            theLatch.release();

            beforeJoin();
            pthreads.join_all();
            // wait for tasks to be processed by consumers,
            // then tell them to stop...not really needed?
            while(chan.size() > 0) sleep(0, 10);
            consumer->mayStop(true); // producers are done, consumers *may* stop...

            cthreads.join_all();
            afterJoin();
        }
        catch (std::exception& e)
        {
            Logger::log(e.what());
        }
        catch (...)
        {
            Logger::log("caught unknown exception");
        }
    }

    // sub-classes overwritables ...
    // monitoring...
    virtual void pcModelCreated(_ProducerTp& producer, _ConsumerTp& consumer) {}
    virtual void beforeJoin() {
        Logger::log("production before join");
    }
    virtual void afterJoin() {
        Logger::log("production after join");
    }
};

template < typename _Tp,
    typename _ChannelTp = Channel<_Tp>,
    typename _ConsumerTp = Consumer<_Tp, _ChannelTp> >
class Consuming
{
protected:
    size_t nConsumers_;
    bool syncStart_, bHidePuttableTakable_;
    _ChannelTp& channel_;
public:

    // bHideTakable indicates derived classes of Consumer
    // do not have custom implementation of Takable interfaces
    Consuming(_ChannelTp& channel, size_t nc = 1,
        bool sc = true, bool bHideTakable = false) //, Gate * gate = NULL)
        : channel_(channel), nConsumers_(nc), syncStart_(sc),
        bHidePuttableTakable_(bHideTakable) //, channelReadGate(gate)
    {}

    void operator()()
    {
        Latch theLatch, noLatch(true);

#if defined(_HAS_PUTTABLETAKABLE)
        Takable<_Tp, _ChannelTp>  takable(channel_); // valid in this function scope

        // consumer is created here, and it is shared by all consumer threads...
        // in a sense the single consumer is the model for a group of threaded live consumers
        _ConsumerTp* consumer = bHidePuttableTakable_
            ? new _ConsumerTp(channel_, syncStart_ ? theLatch : noLatch)
            : new _ConsumerTp(takable, syncStart_ ? theLatch : noLatch);
#else
        _ConsumerTp* consumer = new _ConsumerTp(channel_, syncStart_ ? theLatch : noLatch);
#endif
        std::auto_ptr<_ConsumerTp> consClean(consumer);
        consumer->mayStop(true);
        consumerModelCreated(*consumer);

        boost::thread_group threads;
        try
        {
            for (size_t i = 0; i < nConsumers_; ++i)
                threads.create_thread(*consumer);
        }
        catch (std::exception& err)
        {
            Logger::log(err.what());
        }
        catch (...)
        {
            Logger::log("caught unknown exception");
        }

        theLatch.release();

        beforeJoin();
        threads.join_all();
        afterJoin();
    }

    // sub-classes overwritables ...
    // monitoring...
    virtual void consumerModelCreated(_ConsumerTp& consumer) {}
    virtual void beforeJoin() {
        Logger::log("Consuming before join");
    }
    virtual void afterJoin() {
        Logger::log("Consuming after join");
    }
    // if you set consumer->mayStop(false) in consumerModelCreated, like in ThreadPool
    // this is the only way to stop the pool later by an application...
    virtual void mayStop(bool bMayStop = true)
        { channel_.mayStop(bMayStop); }
};

} // namespace yin

#endif

// Local Variables: **
// mode: c++ **
// End: **
