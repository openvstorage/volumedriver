// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GENERATOR_H_
#define GENERATOR_H_

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

    Generator(const Generator&) = delete;

    Generator& operator=(const Generator&) = delete;

    virtual void next() = 0;
    virtual bool finished() = 0;
    virtual T&   current() = 0;

    std::unique_ptr<std::vector<T> > toList() {
        std::unique_ptr<std::vector<T> > lst(new std::vector<T>);
        while (not finished()) {
            lst->push_back(current());
            next();
        }
        return lst;
    }

    virtual ~Generator() {}
};

class UnknownGeneratorException : public std::exception {
public:
    virtual const char*
    what() const throw ()
    {
        return "Unkown exception raised in generator";
    }
};

template <typename T>
class ProducerFromGenerator : public yin::Producer<T>
{
public:
    //takes ownership of generator
    ProducerFromGenerator(yin::Channel<T>& ch, yin::Latch& l, Generator<T>* generator) :
        yin::Producer<T>(ch, l),
        generator_(generator)
    {}

    void maybeRethrow() {
        if (error_.get()) {
            throw *error_;
        }
    }

    DECLARE_LOGGER("ProducerFromGenerator");

protected:
    //TODO make use of general exception rethrow mechanism across threads
    T produce()
    {
        try
        {
            T tmp = generator_->current();
            generator_->next();
            return tmp;
        }
        catch(std::exception& e) {
            LOG_ERROR("Caught " << e.what());
            error_.reset(new std::exception(e));
            throw; //throwing makes sure production thread is stopped and done() will be called
        }
        catch(...) {
            LOG_ERROR("Caught unkown exception");
            error_.reset(new UnknownGeneratorException());
            throw; //throwing makes sure production thread is stopped and done() will be called
        }
    }

    virtual bool cancel()
    {
        try
        {
            return generator_->finished();
        }
        catch(std::exception& e) {
            LOG_ERROR("Caught " << e.what());
            error_.reset(new std::exception(e));
            return true;
        }
        catch(...) {
            LOG_ERROR("Caught unkown exception");
            error_.reset(new UnknownGeneratorException());
            return true;
        }
    }

    virtual void done() {
        yin::Producer<T>::mayStop();
    }

private:
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
        , producer_(channel, noLatch, generator.release())
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

    void next()
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
    current()
    {
        return current_;
    }

    bool
    finished()
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
    next()
    {
        usleep(usecs_);
        generator_->next();
    }

    bool
    finished()
    {
        return generator_->finished();
    }

    T&
    current()
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
    next()
    {
        current_ = call();
        i++;
    }

    bool
    finished()
    {
        return not unlimited() and i >= maxCount_;
    }

    T&
    current()
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
