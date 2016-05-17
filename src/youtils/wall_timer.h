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

#ifndef WALL_TIMER_H
#define WALL_TIMER_H
#include <sys/time.h>
#include <stdint.h>
namespace youtils
{

class wall_timer
{
public:
    wall_timer()
    {
        restart();
    }

    double
    elapsed() const
    {
        struct timeval x;

        gettimeofday(&x,0);

        /* Perform the carry for the later subtraction by updating Y. */
        if (x.tv_usec < start_time_.tv_usec) {
            long int nsec = (start_time_.tv_usec - x.tv_usec) / 1000000 + 1;
            start_time_.tv_usec -= 1000000 * nsec;
            start_time_.tv_sec += nsec;
        }
        if (x.tv_usec - start_time_.tv_usec > 1000000) {
            long int nsec = (x.tv_usec - start_time_.tv_usec) / 1000000;
            start_time_.tv_usec += 1000000 * nsec;
            start_time_.tv_sec -= nsec;
        }

        /* Compute the time remaining to wait.
           `tv_usec' is certainly positive. */
        return static_cast<double>(x.tv_sec - start_time_.tv_sec) +
            (static_cast<double>((double)x.tv_usec - (double)start_time_.tv_usec)
             /    (double)1000000.0);
    }

    void
    restart()
    {
        gettimeofday(&start_time_,0);
    }

private:
    // Y42 doesn't make much sense to make this mutable if restart isn't made const.
    mutable struct timeval start_time_;

    // Y42 Can't find these anywhere ???
    double
    elapsed_max() const;

    double
    elapsed_min() const;
};

class wall_timer2
{
public:
    wall_timer2()
    {
        restart();
    }

    double
    elapsed_in_seconds() const
    {
        return double (get_uint64_time() - start_time_in_microsec_) / (double)micro_sec_multiplier;
    }

    void
    restart()
    {
        start_time_in_microsec_ = get_uint64_time();
    }

private:
    inline uint64_t
    get_uint64_time() const
    {
        struct timeval now;
        gettimeofday(&now,0);
        return (now.tv_sec * micro_sec_multiplier) + now.tv_usec;
    }

    uint64_t start_time_in_microsec_;
    const static uint64_t micro_sec_multiplier = 1000000;


};

}
#endif //WALL_TIMER_H

// Local Variables: **
// End: **
