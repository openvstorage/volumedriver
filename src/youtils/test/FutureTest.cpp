// Copyright (C) 2017 iNuron NV
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

#include "../InlineExecutor.h"

#include <boost/exception_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/future.hpp>

#include <gtest/gtest.h>

namespace youtilstest
{

using namespace youtils;

// observation (boost 1.62): a new thread is spawned for a continuation on
// a ready future (implicitly using launch policy "none").
TEST(FutureTest, default_continuation_of_ready_future)
{
    auto f(boost::make_ready_future<bool>(true));
    const boost::thread::id tid(boost::this_thread::get_id());
    boost::future<void>
        g(f.then([tid](boost::future<bool> f)
                 {
                     EXPECT_TRUE(f.is_ready());
                     EXPECT_TRUE(f.get());
                     EXPECT_NE(tid,
                               boost::this_thread::get_id());
                 }));

    EXPECT_FALSE(f.valid());
    g.wait();
}

TEST(FutureTest, inline_continuation_of_ready_future)
{
    auto f(boost::make_ready_future<bool>(true));
    const boost::thread::id tid(boost::this_thread::get_id());

    boost::future<void>
        g(f.then(InlineExecutor::get(),
                 [tid](boost::future<bool> f)
                 {
                     EXPECT_TRUE(f.is_ready());
                     EXPECT_TRUE(f.get());
                     EXPECT_EQ(tid,
                               boost::this_thread::get_id());
                 }));

    EXPECT_FALSE(f.valid());
    EXPECT_TRUE(g.is_ready());
    g.wait();
}

TEST(FutureTest, continuation_of_pending_future_in_callback_thread)
{
    boost::promise<boost::thread::id> p;
    boost::future<boost::thread::id> f(p.get_future());
    boost::future<void> g(f.then(InlineExecutor::get(),
                                 [](boost::future<boost::thread::id> tid)
                                 {
                                     EXPECT_TRUE(tid.valid());
                                     EXPECT_TRUE(tid.is_ready());
                                     EXPECT_EQ(tid.get(),
                                               boost::this_thread::get_id());
                                 }));

    EXPECT_TRUE(g.valid());
    EXPECT_FALSE(g.is_ready());

    boost::mutex mutex;
    boost::unique_lock<decltype(mutex)> u(mutex);
    boost::condition_variable cond;
    bool done = false;

    boost::thread thread([&cond,
                          &done,
                          &mutex,
                          p = std::move(p)]() mutable
                         {
                             boost::lock_guard<decltype(mutex)> g(mutex);
                             p.set_value(boost::this_thread::get_id());
                             done = true;
                             cond.notify_one();
                         });

    cond.wait(u,
              [&done]
              {
                  return done == true;
              });

    thread.join();

    EXPECT_TRUE(g.is_ready());
    g.get();
}

TEST(FutureTest, exceptional_continuation)
{
    boost::promise<void> p;

    // there's no boost::make_exception_ptr
    try
    {
        throw std::logic_error("only kidding");
    }
    catch (...)
    {
        p.set_exception(boost::current_exception());
    }

    boost::future<void> f(p.get_future());
    boost::future<void> g(f.then(InlineExecutor::get(),
                                 [](boost::future<void> f)
                                 {
                                     f.get();
                                 }));

    bool seen = false;
    boost::future<void> h(g.then(InlineExecutor::get(),
                                 [&seen](boost::future<void> f)
                                 {
                                     seen = true;
                                     f.get();
                                 }));

    EXPECT_TRUE(h.has_exception());
    EXPECT_THROW(h.get(),
                 std::logic_error);
    EXPECT_TRUE(seen);
}

TEST(FutureTest, exceptional_continuation_again)
{
    boost::promise<void> p;
    p.set_exception(std::logic_error("just kidding"));

    boost::future<void> f(p.get_future());
    boost::future<void> g(f.then(InlineExecutor::get(),
                                 [](boost::future<void> f)
                                 {
                                     f.get();
                                 }));

    EXPECT_TRUE(g.has_exception());
    EXPECT_THROW(g.get(),
                 std::logic_error);
}

// Good to know: promise.set_exception(std::make_exception_ptr(...))
// will lead to future.get() throwing an std::exception_ptr and
// *not* (as one might expect) the exception inside the exception_ptr
TEST(FutureTest, exceptional_continuation_std_exception_ptr)
{
    boost::promise<void> p;
    p.set_exception(std::make_exception_ptr(std::logic_error("just kidding")));

    boost::future<void> f(p.get_future());
    boost::future<void> g(f.then(InlineExecutor::get(),
                                 [](boost::future<void> f)
                                 {
                                     f.get();
                                 }));

    EXPECT_TRUE(g.has_exception());

    try
    {
        g.get();
    }
    catch (std::exception_ptr& p)
    {}
}

TEST(FutureTest, closures)
{
    struct S
    {
        S(bool& dr)
            : dtor_ran(dr)
        {
            dtor_ran = false;
        }

        ~S()
        {
            dtor_ran = true;
        }

        bool& dtor_ran;
        boost::future<void> future;
    };

    using SPtr = std::shared_ptr<S>;

    bool dtor_ran = false;

    {
        auto sptr(std::make_shared<S>(dtor_ran));
        auto f(boost::make_ready_future(sptr));

        sptr->future = f.then(InlineExecutor::get(),
                              [sptr](boost::future<SPtr> f) mutable -> void
                              {
                                  sptr = nullptr;
                                  EXPECT_FALSE(f.get()->dtor_ran);
                              });
    }

    EXPECT_TRUE(dtor_ran);
}

}
