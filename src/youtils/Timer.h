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

#ifndef YT_TIMER_H_
#define YT_TIMER_H_

#include <boost/chrono.hpp>

namespace youtils
{

template<typename Clock>
class Timer
{
public:
    Timer()
        : start_(Clock::now())
    {}

    typename Clock::duration
    elapsed() const
    {
        return Clock::now() - start_;
    }

private:
    typename Clock::time_point start_;
};

using SteadyTimer = Timer<boost::chrono::steady_clock>;

}

#endif // !YT_TIMER_H_
