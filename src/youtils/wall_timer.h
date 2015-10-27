// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
