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

#ifndef GENERATOR_H_
#define GENERATOR_H_

#include "Assert.h"
#include "Catchers.h"
#include "Logging.h"

#include <procon/ProCon.h>

namespace youtils
{

template <typename T>
class Generator
{
public:
    Generator() = default;

    virtual ~Generator() = default;

    Generator(const Generator&) = delete;

    Generator&
    operator=(const Generator&) = delete;

    virtual void
    next() = 0;

    virtual bool
    finished() = 0;

    virtual T&
    current() = 0;

    std::unique_ptr<std::vector<T>>
    toList()
    {
        std::unique_ptr<std::vector<T> > lst(new std::vector<T>);
        while (not finished())
        {
            lst->push_back(current());
            next();
        }
        return lst;
    }
};

class UnknownGeneratorException
    : public std::exception
{
public:
    virtual const char*
    what() const throw ()
    {
        return "Unkown exception raised in generator";
    }
};

template <typename T>
class ProducerFromGenerator
    : public yin::Producer<T>
{
public:
    ProducerFromGenerator(yin::Channel<T>& ch,
                          yin::Latch& l,
                          std::unique_ptr<Generator<T>> generator)
        : yin::Producer<T>(ch, l)
        , generator_(std::move(generator))
    {}

    void
    maybeRethrow()
    {
        if (error_.get())
        {
            throw *error_;
        }
    }

protected:
    //TODO make use of general exception rethrow mechanism across threads
    T
    produce() override final
    {
        try
        {
            T tmp = generator_->current();
            generator_->next();
            return tmp;
        }
        catch(std::exception& e)
        {
            LOG_ERROR("Caught " << e.what());
            error_.reset(new std::exception(e));
            throw; //throwing makes sure production thread is stopped and done() will be called
        }
        catch(...)
        {
            LOG_ERROR("Caught unkown exception");
            error_.reset(new UnknownGeneratorException());
            throw; //throwing makes sure production thread is stopped and done() will be called
        }
    }

    virtual bool
    cancel() override final
    {
        try
        {
            return generator_->finished();
        }
        catch(std::exception& e)
        {
            LOG_ERROR("Caught " << e.what());
            error_.reset(new std::exception(e));
            return true;
        }
        catch(...)
        {
            LOG_ERROR("Caught unkown exception");
            error_.reset(new UnknownGeneratorException());
            return true;
        }
    }

    virtual void
    done() override final
    {
        yin::Producer<T>::mayStop();
    }

private:
    DECLARE_LOGGER("ProducerFromGenerator");

    std::unique_ptr<Generator<T> > generator_;
    std::unique_ptr<std::exception> error_;
};


template <typename T>
class ThreadedGenerator
    : public Generator<T>
{
public:
    ThreadedGenerator(std::unique_ptr<Generator<T>> generator,
                      uint32_t bufferSize)
        : channel(bufferSize)
        , noLatch(true)
        , producer_(channel,
                    noLatch,
                    std::move(generator))
        , thread_(boost::ref(producer_))
        , finished_(false)
    {
        try
        {
            next();
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Exception in constructor: " << EWHAT);
                stop_();
                throw;
            });
    }

    ~ThreadedGenerator()
    {
        stop_();
    }

    void
    next() override final
    {
        try
        {
            current_ = channel.poll();
        }
        catch(yin::pc_exception& e)
        { //indicates end of queue
            finished_ = true;
            stop_();
            producer_.maybeRethrow();
        }
        CATCH_STD_ALL_LOG_RETHROW("caught exception");
    }

    T&
    current() override final
    {
        return current_;
    }

    bool
    finished() override final
    {
        return finished_;
    }

private:
    DECLARE_LOGGER("ThreadedGenerator");

    T current_;
    yin::Channel<T> channel;
    yin::Latch noLatch;
    ProducerFromGenerator<T> producer_;
    boost::thread thread_;
    bool finished_;

    void stop_()
    {
        channel.mayStop();
        thread_.join();
    }
};

template<typename T>
class PrefetchGenerator
    : public Generator<T>
{
public:
    explicit PrefetchGenerator(std::unique_ptr<Generator<T>> generator)
    {
        while (not generator->finished())
        {
            queue_.emplace_back(generator->current());
            generator->next();
        }
    }

    ~PrefetchGenerator() = default;

    bool
    finished() override final
    {
        return queue_.empty();
    }

    T&
    current() override final
    {
        VERIFY(not queue_.empty());
        return queue_.front();
    }

    void
    next() override final
    {
        VERIFY(not queue_.empty());
        queue_.pop_front();
    }

    size_t
    size() const
    {
        return queue_.size();
    }

private:
    DECLARE_LOGGER("PrefetchGenerator");

    std::deque<T> queue_;
};

//auxiliary stuff for testing etc.

template <typename T>
class DelayedGenerator
    : public Generator<T>
{
public:
    DelayedGenerator(std::unique_ptr<Generator<T>> generator,
                     uint64_t usecs)
        : generator_(std::move(generator))
        , usecs_(usecs)
    {}

    void
    next() override final
    {
        usleep(usecs_);
        generator_->next();
    }

    bool
    finished() override final
    {
        return generator_->finished();
    }

    T&
    current() override final
    {
        return generator_->current();
    }

private:
    std::unique_ptr<Generator<T> > generator_;
    uint64_t usecs_;
};

template <typename T, typename Callable>
class RepeatGenerator
    : public Generator<T>
{
public:
    RepeatGenerator(Callable c)
        : i(0)
        , maxCount_(-1)
        , call(c)
    {
        next();
    }

    RepeatGenerator(Callable c,
                    uint64_t maxCount)
        : i(0)
        , maxCount_(maxCount)
        , call(c)
    {
        next();
    }

    void
    next() override final
    {
        current_ = call();
        i++;
    }

    bool
    finished() override final
    {
        return not unlimited() and i >= maxCount_;
    }

    T&
    current() override final
    {
        return current_;
    }

private:
    bool unlimited()
    {
        return maxCount_ < 0;
    }

    T current_;
    int i;
    int maxCount_;
    Callable call;
};

}

#endif /* GENERATOR_H_ */

// Local Variables: **
// mode: c++ **
// End: **
