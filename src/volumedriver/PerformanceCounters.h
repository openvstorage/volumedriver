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

#ifndef PERFORMANCE_COUNTERS_H
#define PERFORMANCE_COUNTERS_H

#include <mutex>

#include <boost/array.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/nvp.hpp>

#include <youtils/Serialization.h>
#include <youtils/SpinLock.h>

namespace volumedriver
{

template<typename T>
struct NoBucketTraits
{
    static constexpr size_t max_buckets = 0;

    static const boost::array<T, max_buckets>&
    bounds()
    {
        static const boost::array<T, max_buckets> b;
        return b;
    }
};

template<typename T, typename BuckTraits = NoBucketTraits<T>>
class PerformanceCounter
{
private:
    mutable fungi::SpinLock lock_;
    uint64_t events_;
    T sum_;
    T sqsum_;
    T min_;
    T max_;
    boost::array<uint64_t, BuckTraits::max_buckets> buckets_;

public:
    using Type = T;
    using BucketTraits = BuckTraits;
    static constexpr size_t max_buckets = BucketTraits::max_buckets;

    PerformanceCounter()
        : events_(0)
        , sum_(0)
        , sqsum_(0)
        , min_(std::numeric_limits<T>::max())
        , max_(std::numeric_limits<T>::min())
    {
        buckets_.fill(0);
    }

    PerformanceCounter(const PerformanceCounter& other)
        : events_(other.events())
        , sum_(other.sum())
        , sqsum_(other.sum_of_squares())
        , min_(other.min())
        , max_(other.max())
        , buckets_(other.buckets())
    {}

    PerformanceCounter&
    operator=(const PerformanceCounter& other)
    {
        if (this != &other)
        {
            std::lock_guard<decltype(lock_)> g(lock_);
            std::lock_guard<decltype(lock_)> h(other.lock_);

            events_ = other.events_;
            sum_ = other.sum_;
            sqsum_ = other.sqsum_;
            min_ = other.min_;
            max_ = other.max_;
            buckets_ = other.buckets_;
        }

        return *this;
    }

    PerformanceCounter&
    operator+=(const PerformanceCounter& other)
    {
        std::lock_guard<decltype(lock_)> g(lock_);
        if (this == &other)
        {
            events_ *= 2;
            sum_ *= 2;
            sqsum_ *= 2;
            for (auto& b : buckets_)
            {
                b *= 2;
            }
            return *this;
        }
        else
        {
            std::lock_guard<decltype(lock_)> h(other.lock_);
            events_ += other.events_;
            sum_ += other.sum_;
            sqsum_ += other.sqsum_;
            min_ = std::min<T>(min_,
                               other.min_);
            max_ = std::max<T>(max_,
                               other.max_);

            for (size_t i = 0; i < buckets_.max_size(); ++i)
            {
                buckets_[i] += other.buckets_[i];
            }
            return *this;
        }
    }

    friend PerformanceCounter
    operator+(PerformanceCounter lhs,
              const PerformanceCounter& rhs)
    {
        lhs += rhs;
        return lhs;
    }

    void
    count(const T t)
    {
        std::lock_guard<decltype(lock_)> g(lock_);

        events_++;
        sum_ += t;
        sqsum_ += t * t;
        if (min_ > t)
        {
            min_ = t;
        }
        if (max_ < t)
        {
            max_ = t;
        }

        for (size_t i = 0; i < buckets_.max_size(); ++i)
        {
            if (t < BucketTraits::bounds()[i])
            {
                ++buckets_[i];
                break;
            }
        }
    }

    void
    reset()
    {
        std::lock_guard<decltype(lock_)> g(lock_);

        events_ = 0;
        sum_ = 0;
        sqsum_ = 0;
        min_ = std::numeric_limits<T>::max();
        max_ = std::numeric_limits<T>::min();
        buckets_.fill(0);
    }

    uint64_t
    events() const
    {
        std::lock_guard<decltype(lock_)> g(lock_);
        return events_;
    }

    T
    sum() const
    {
        std::lock_guard<decltype(lock_)> g(lock_);
        return sum_;
    }

    T
    sum_of_squares() const
    {
        std::lock_guard<decltype(lock_)> g(lock_);
        return sqsum_;
    }

    T
    min() const
    {
        std::lock_guard<decltype(lock_)> g(lock_);
        return min_;
    }

    T
    max() const
    {
        std::lock_guard<decltype(lock_)> g(lock_);
        return max_;
    }

    decltype(buckets_)
        buckets() const
    {
        std::lock_guard<decltype(lock_)> g(lock_);
        return buckets_;
    }

    static const boost::array<T, BucketTraits::max_buckets>&
    bucket_bounds()
    {
        return BucketTraits::bounds();
    }

    bool
    operator==(const PerformanceCounter& other) const
    {
        return
            events() == other.events() and
            sum() == other.sum() and
            sum_of_squares() == other.sum_of_squares() and
            min() == other.min() and
            max() == other.max() and
            buckets() == other.buckets();
    }

    std::map<T, uint64_t>
    distribution() const
    {
        const auto bounds(BucketTraits::bounds());
        const auto bucks(buckets());

        std::map<T, uint64_t> m;

        for (size_t i = 0; i < bounds.max_size(); ++i)
        {
            m[bounds[i]] = bucks[i];
        }

        return m;
    }

private:
    friend class boost::serialization::access;
    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<typename Archive>
    void
    load(Archive& ar,
         const unsigned version)
    {
        std::lock_guard<decltype(lock_)> g(lock_);

        ar & boost::serialization::make_nvp("events",
                                            events_);
        ar & boost::serialization::make_nvp("sum",
                                            sum_);
        ar & boost::serialization::make_nvp("sqsum",
                                            sqsum_);
        if (version > 0)
        {
            ar & boost::serialization::make_nvp("min",
                                                min_);
            ar & boost::serialization::make_nvp("max",
                                                max_);
        }

        if (version > 1)
        {
            if (BucketTraits::max_buckets > 0)
            {
                ar & boost::serialization::make_nvp("buckets",
                                                    buckets_);
            }
        }
    }

    template<typename Archive>
    void
    save(Archive& ar,
         const unsigned /* version */) const
    {
        const uint64_t evts = events();
        ar & boost::serialization::make_nvp("events",
                                            evts);
        const T s = sum();
        ar & boost::serialization::make_nvp("sum",
                                            s);

        const T sq = sum_of_squares();
        ar & boost::serialization::make_nvp("sqsum",
                                            sq);

        const T mn = min();
        ar & boost::serialization::make_nvp("min",
                                            mn);
        const T mx = max();
        ar & boost::serialization::make_nvp("max",
                                            mx);

        if (BucketTraits::max_buckets > 0)
        {
            const decltype(buckets_) bs = buckets();
            ar & boost::serialization::make_nvp("buckets",
                                                bs);
        }
    }
};

struct RequestSizeBucketTraits
{
    static constexpr size_t max_buckets = 8;

    static const boost::array<uint64_t, max_buckets>&
    bounds()
    {
        static const boost::array<uint64_t, max_buckets> buckets = { 4 * 1024,
                                                                     8 * 1024,
                                                                     16 * 1024,
                                                                     32 * 1024,
                                                                     64 * 1024,
                                                                     128 * 1024,
                                                                     256 * 1024,
                                                                     512 * 1024,
        };

        return buckets;
    }
};

struct RequestUSecsBucketTraits
{
    static constexpr size_t max_buckets = 10;

    static const boost::array<uint64_t, max_buckets>&
    bounds()
    {
        static const boost::array<uint64_t, max_buckets> buckets = { 100,
                                                                     250,
                                                                     500,
                                                                     750,
                                                                     1000,
                                                                     2000,
                                                                     5000,
                                                                     10000,
                                                                     100000,
                                                                     1000000,
        };

        return buckets;
    }
};

using RequestSizeCounter = PerformanceCounter<uint64_t, RequestSizeBucketTraits>;
using RequestUSecsCounter = PerformanceCounter<uint64_t, RequestUSecsBucketTraits>;

struct PerformanceCounters
{
    RequestSizeCounter write_request_size;
    RequestUSecsCounter write_request_usecs;

    RequestSizeCounter unaligned_write_request_size;

    RequestUSecsCounter backend_write_request_usecs;
    RequestSizeCounter backend_write_request_size;

    RequestSizeCounter read_request_size;
    RequestUSecsCounter read_request_usecs;

    RequestSizeCounter unaligned_read_request_size;
    RequestSizeCounter backend_read_request_size;
    RequestUSecsCounter backend_read_request_usecs;

    RequestUSecsCounter sync_request_usecs;

    PerformanceCounters() = default;

    ~PerformanceCounters() = default;

    PerformanceCounters(const PerformanceCounters&) = default;

    PerformanceCounters&
    operator=(const PerformanceCounters&) = default;

    bool
    operator==(const PerformanceCounters& other) const
    {
#define EQ(x)                                   \
        x == other.x

        return
            EQ(write_request_size) and
            EQ(write_request_usecs) and
            EQ(unaligned_write_request_size) and
            EQ(backend_write_request_usecs) and
            EQ(backend_write_request_size) and
            EQ(read_request_size) and
            EQ(read_request_usecs) and
            EQ(unaligned_read_request_size) and
            EQ(backend_read_request_size) and
            EQ(backend_read_request_usecs) and
            EQ(sync_request_usecs);

#undef EQ
    }

    PerformanceCounters&
    operator+=(const PerformanceCounters& other)
    {
#define ADD(x)                                  \
        x += other.x

        ADD(write_request_size);
        ADD(write_request_usecs);
        ADD(unaligned_write_request_size);
        ADD(backend_write_request_usecs);
        ADD(backend_write_request_size);
        ADD(read_request_size);
        ADD(read_request_usecs);
        ADD(unaligned_read_request_size);
        ADD(backend_read_request_size);
        ADD(backend_read_request_usecs);
        ADD(sync_request_usecs);

        return *this;
#undef ADD
    }

    friend PerformanceCounters
    operator+(PerformanceCounters lhs,
              const PerformanceCounters& rhs)
    {
        lhs += rhs;
        return lhs;
    }

    void
    reset_all_counters()
    {
        write_request_size.reset();
        write_request_usecs.reset();

        unaligned_write_request_size.reset();

        backend_write_request_usecs.reset();
        backend_write_request_size.reset();

        read_request_size.reset();
        read_request_usecs.reset();

        unaligned_read_request_size.reset();
        backend_read_request_size.reset();
        backend_read_request_usecs.reset();

        sync_request_usecs.reset();
    }

    template<typename Archive>
    void
    serialize(Archive& ar,
              const unsigned /* version */)
    {
#define S(x)                                    \
        ar & BOOST_SERIALIZATION_NVP(x)

        S(write_request_size);
        S(write_request_usecs);
        S(unaligned_write_request_size);
        S(backend_write_request_usecs);
        S(backend_write_request_size);
        S(read_request_size);
        S(read_request_usecs);
        S(unaligned_read_request_size);
        S(backend_read_request_size);
        S(backend_read_request_usecs);
        S(sync_request_usecs);

#undef S
    }
};

}

BOOST_CLASS_VERSION(volumedriver::RequestSizeCounter, 2);
BOOST_CLASS_VERSION(volumedriver::RequestUSecsCounter, 2);
BOOST_CLASS_VERSION(volumedriver::PerformanceCounters, 0);

#endif // PERFORMANCE_COUNTERS_H
