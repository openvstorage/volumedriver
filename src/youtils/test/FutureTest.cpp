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

#include "../FutureUtils.h"
#include "../InlineExecutor.h"

#include <boost/chrono.hpp>
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

TEST(FutureTest, boost_when_all)
{
    std::vector<boost::promise<size_t>> promises(3);
    std::vector<boost::future<size_t>> futures;

    futures.reserve(promises.size());
    for (auto& p : promises)
    {
        futures.push_back(p.get_future());
    }

    auto future(boost::when_all(futures.begin(),
                                futures.end()));

    ASSERT_TRUE(future.valid());
    ASSERT_FALSE(future.is_ready());

    for (size_t i = 0; i < promises.size(); ++i)
    {
        promises[i].set_value(i);
    }

    auto vec(future.get());

    ASSERT_EQ(promises.size(),
              vec.size());

    for (size_t i = 0; i < vec.size(); ++i)
    {
        EXPECT_TRUE(vec[i].is_ready());
        EXPECT_EQ(i,
                  vec[i].get());
    }
}

TEST(FutureTest, when_all)
{
    for (size_t i = 0; i < 3; ++i)
    {
        std::vector<boost::promise<size_t>> promises(i);
        std::vector<boost::future<size_t>> futures;

        futures.reserve(promises.size());
        for (auto& p : promises)
        {
            futures.push_back(p.get_future());
        }

        auto future(FutureUtils::when_all(InlineExecutor::get(),
                                          futures.begin(),
                                          futures.end()));

        ASSERT_TRUE(future.valid());
        if (i == 0)
        {
            ASSERT_TRUE(future.is_ready());
        }
        else
        {
            ASSERT_FALSE(future.is_ready());
        }

        for (size_t i = 0; i < promises.size(); ++i)
        {
            promises[i].set_value(i);
        }

        ASSERT_TRUE(future.is_ready());
        auto vec(future.get());

        ASSERT_EQ(promises.size(),
                  vec.size());

        for (size_t i = 0; i < vec.size(); ++i)
        {
            EXPECT_EQ(i,
                      vec[i]);
        }
    }
}

TEST(FutureTest, when_all_again)
{
    const size_t count = 4;
    std::vector<boost::future<boost::thread::id>> futures;
    futures.reserve(count);

    boost::mutex mutex;
    boost::condition_variable cond;
    bool go = false;
    boost::future<void> future;

    {
        boost::lock_guard<decltype(mutex)> g(mutex);

        for (size_t i = 0; i < count; ++i)
        {
            futures.push_back(boost::async(boost::launch::async,
                                           [&cond,
                                            &go,
                                            &mutex]() -> boost::thread::id
                                           {
                                               boost::unique_lock<decltype(mutex)> u(mutex);
                                               cond.wait(u,
                                                         [&go]() -> bool
                                                         {
                                                             return go;
                                                         });
                                               return boost::this_thread::get_id();
                                           }));
        }

        using VecFuture = boost::future<std::vector<boost::thread::id>>;

        const boost::thread::id main(boost::this_thread::get_id());
        future = FutureUtils::when_all(InlineExecutor::get(),
                                       futures.begin(),
                                       futures.end())
            .then(InlineExecutor::get(),
                  [count,
                   &main](VecFuture f) -> void
                  {
                      std::vector<boost::thread::id> vec(f.get());
                      ASSERT_EQ(count,
                                vec.size());

                      bool ok = false;
                      for (auto& i : vec)
                      {
                          EXPECT_NE(i,
                                    main);
                          if (i == boost::this_thread::get_id())
                          {
                              ok = true;
                          }
                      }

                      EXPECT_TRUE(ok);
                      EXPECT_NE(main,
                                boost::this_thread::get_id());
                  });

        go = true;
        cond.notify_all();
    }

    ASSERT_TRUE(future.valid());
    while (not future.is_ready())
    {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
    }

    future.get();
}

TEST(FutureTest, exceptional_when_all)
{
    for (size_t i = 1; i < 3; ++i)
    {
        std::vector<boost::promise<size_t>> promises(i);
        std::vector<boost::future<size_t>> futures;

        futures.reserve(promises.size());
        for (auto& p : promises)
        {
            futures.push_back(p.get_future());
        }

        auto future(FutureUtils::when_all(InlineExecutor::get(),
                                          futures.begin(),
                                          futures.end()));

        ASSERT_TRUE(future.valid());
        ASSERT_FALSE(future.is_ready());

        for (size_t i = 0; i < promises.size(); ++i)
        {
            if (i == 0)
            {
                promises[i].set_exception(std::logic_error("just a test"));
            }
            else
            {
                promises[i].set_value(i);
            }
        }

        EXPECT_TRUE(future.has_exception());
        EXPECT_THROW(future.get(),
                     std::logic_error);
    }
}

TEST(FutureTest, when_all_void)
{
    for (size_t i = 0; i < 3; ++i)
    {
        std::vector<boost::promise<void>> promises(i);
        std::vector<boost::future<void>> futures;
        futures.reserve(promises.size());

        for (auto& p : promises)
        {
            futures.push_back(p.get_future());
        }

        bool done = false;
        boost::future<void> f(FutureUtils::when_all(InlineExecutor::get(),
                                                    futures.begin(),
                                                    futures.end())
                              .then(InlineExecutor::get(),
                                    [&done](boost::future<void> f)
                                    {
                                        EXPECT_TRUE(f.valid());
                                        EXPECT_FALSE(f.has_exception());
                                        EXPECT_TRUE(f.is_ready());
                                        f.get();
                                        done = true;
                                    }));

        for (auto& p : promises)
        {
            p.set_value();
        }

        EXPECT_TRUE(done);
        EXPECT_TRUE(f.valid());
        EXPECT_TRUE(f.is_ready());
        EXPECT_FALSE(f.has_exception());
    }
}

TEST(FutureTest, exceptional_when_all_void)
{
    for (size_t i = 1; i < 3; ++i)
    {
        std::vector<boost::promise<void>> promises(i);
        std::vector<boost::future<void>> futures;
        futures.reserve(promises.size());

        for (auto& p : promises)
        {
            futures.push_back(p.get_future());
        }

        bool done = false;
        boost::future<void> f(FutureUtils::when_all(InlineExecutor::get(),
                                                    futures.begin(),
                                                    futures.end())
                              .then(InlineExecutor::get(),
                                    [&done](boost::future<void> f)
                                    {
                                        EXPECT_TRUE(f.valid());
                                        EXPECT_TRUE(f.is_ready());
                                        EXPECT_TRUE(f.has_exception());
                                        f.get();
                                        done = true;
                                    }));

        bool fst = true;
        for (auto& p : promises)
        {
            if (fst)
            {
                p.set_exception(std::logic_error("just a test"));
                fst = false;
            }
            else
            {
                p.set_value();
            }
        }

        EXPECT_FALSE(done);
        EXPECT_TRUE(f.valid());
        EXPECT_TRUE(f.has_exception());
        EXPECT_THROW(f.get(),
                     std::logic_error);
    }
}

}
