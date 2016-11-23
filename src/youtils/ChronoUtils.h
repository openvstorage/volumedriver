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

#ifndef YT_CHRONO_UTILS_H_
#define YT_CHRONO_UTILS_H_

#include "Assert.h"
#include "Logging.h"

#include <boost/chrono.hpp>

namespace youtils
{

template<typename>
struct ToAbsTimeSpec;

template<>
struct ToAbsTimeSpec<boost::chrono::steady_clock::duration>
{
    using Clock = boost::chrono::steady_clock;
    using Duration = Clock::duration;

    static timespec
    abs_timespec(const Duration& t)
    {
        using namespace boost::chrono;
        using SysClock = system_clock;

        const auto abs(duration_cast<nanoseconds>((SysClock::now() + t).time_since_epoch()));
        timespec ts;
        ts.tv_sec = abs.count() / 1000000000;
        ts.tv_nsec = abs.count() % 1000000000;

        return ts;
    }
};

template<>
struct ToAbsTimeSpec<boost::chrono::steady_clock::time_point>
{
    using Clock = boost::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static timespec
    abs_timespec(const TimePoint& t)
    {
        return ToAbsTimeSpec<Clock::duration>::abs_timespec(t - Clock::now());
    }
};

struct ChronoUtils
{
    template<typename T, typename Converter = ToAbsTimeSpec<T>>
    static timespec
    abs_timespec(const T& t)
    {
        return Converter::abs_timespec(t);
    }

    DECLARE_LOGGER("ChronoUtils");
};

}

#endif // !YT_CHRONO_UTILS_H_
