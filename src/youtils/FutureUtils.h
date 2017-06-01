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

#ifndef YT_FUTURE_UTILS_H_
#define YT_FUTURE_UTILS_H_

#include "InlineExecutor.h"

#include <iterator>
#include <memory>
#include <type_traits>

#include <boost/exception_ptr.hpp>
#include <boost/thread/future.hpp>
#include <boost/type_traits.hpp>

namespace youtils
{

struct FutureUtils
{
    // boost's `when_all' (as of 1.62) returns a future<vector<future<T>>> and
    // does not give control over the executor
    // TODO: move locking to a policy
    template<typename InputIterator,
             typename Executor>
    static std::enable_if_t<std::is_same<typename std::iterator_traits<InputIterator>::value_type::value_type,
                                         void>::value,
                            boost::future<void>>
    when_all(Executor& executor,
             InputIterator first,
             InputIterator last)
    {
        using InputFuture = boost::future<void>;

        if (first == last)
        {
            return boost::make_ready_future();
        }

        if (std::distance(first, last) == 1)
        {
            return std::move(*first);
        }

        struct Collector
        {
            boost::promise<void> promise;
            boost::mutex mutex;
            std::vector<InputFuture> continuations;
            boost::exception_ptr exception;

            Collector(size_t size)
            {
                continuations.reserve(size);
            }

            ~Collector()
            {
                if (exception)
                {
                    promise.set_exception(exception);
                }
                else
                {
                    promise.set_value();
                }
            }
        };

        auto collector(std::make_shared<Collector>(std::distance(first, last)));

        for (; first != last; ++first)
        {
            auto fun([collector](InputFuture&& f) mutable -> void
                     {
                         // break the circular dependency between `collector' and
                         // the closure (in `collector->continuations'):
                         auto c(std::move(collector));

                         try
                         {
                             f.get();
                         }
                         catch (...)
                         {
                             boost::lock_guard<decltype(c->mutex)> g(c->mutex);

                             c->exception = boost::current_exception();
                             throw;
                         }
                     });

            collector->continuations.push_back(first->then(executor,
                                                           std::move(fun)));
        }

        return collector->promise.get_future();
    }

    template<typename InputIterator,
             typename Executor>
    static std::enable_if_t<not std::is_same<typename std::iterator_traits<InputIterator>::value_type::value_type,
                                             void>::value,
                            boost::future<std::vector<typename std::iterator_traits<InputIterator>::value_type::value_type>>>
    when_all(Executor& executor,
             InputIterator first,
             InputIterator last)
    {
        using T = typename std::iterator_traits<InputIterator>::value_type::value_type;
        using Vector = std::vector<T>;
        using InputFuture = boost::future<T>;

        static_assert(std::is_same<typename std::iterator_traits<InputIterator>::value_type,
                                   InputFuture>::value,
                      "this only supports iterators with boost::future<T> values");
        // keep it simple for starters: the collector's Vector is populated with default
        // constructed objects and once the futures are resolved one by one the actually
        // values are copied in. Obviously not a good idea with expensive default ctor's /
        // assignment operators.
        static_assert(std::is_default_constructible<T>::value or
                      // std::is_trivially_default_construcible is not available
                      // with g++-4.9?
                      boost::has_trivial_default_constructor<T>::value,
                      "this only works with default constructible types");
        static_assert(std::is_nothrow_assignable<T, T>::value or
                      // std::is_trivially_assignable is not available with
                      // g++-4.9?
                      boost::has_trivial_assign<T>::value,
                      "this only works with trivially / nothrow assignable types");

        if (first == last)
        {
            return boost::make_ready_future<Vector>();
        }

        struct Collector
        {
            Vector vec;
            boost::promise<Vector> promise;
            boost::mutex mutex;
            std::vector<InputFuture> continuations;
            boost::exception_ptr exception;

            Collector(size_t size)
                : vec(size)
            {
                continuations.reserve(size);
            }

            ~Collector()
            {
                if (exception)
                {
                    promise.set_exception(exception);
                }
                else
                {
                    promise.set_value(std::move(vec));
                }
            }
        };

        auto collector(std::make_shared<Collector>(std::distance(first, last)));

        for (size_t i = 0; first != last; ++first, ++i)
        {
            auto fun([collector,
                      i](InputFuture&& f) mutable -> T
                     {
                         // break the circular dependency between `collector' and
                         // the closure (in `collector->continuations'):
                         auto c(std::move(collector));

                         boost::lock_guard<decltype(c->mutex)> g(c->mutex);

                         try
                         {
                             c->vec[i] = f.get();
                             return c->vec[i];
                         }
                         catch (...)
                         {
                             c->exception = boost::current_exception();
                             throw;
                         }
                     });

            collector->continuations.push_back(first->then(executor,
                                                           std::move(fun)));
        }

        return collector->promise.get_future();
    }
};

}

#endif // !YT_FUTURE_UTILS_H_
