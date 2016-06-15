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

#ifndef CPU_TIMER_H
#define CPU_TIMER_H

#include <time.h>
#include "Assert.h"

namespace youtils
{

class cpu_timer
{
public:
    cpu_timer()
    {
        restart();
    }

    double
    elapsed()
    {
        timespec tspec;
        int result = clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                   &tspec);
        VERIFY(result == 0);

        if (tspec.tv_nsec < start_time_.tv_nsec)
        {
            long int nsec = (start_time_.tv_nsec - tspec.tv_nsec) / 1000000000 + 1;
            start_time_.tv_nsec -= 1000000000 * nsec;
            start_time_.tv_sec += nsec;
        }
        if (tspec.tv_nsec - start_time_.tv_nsec > 1000000000)
        {
            long int nsec = (tspec.tv_nsec - start_time_.tv_nsec) / 1000000000;
            start_time_.tv_nsec += 1000000000 * nsec;
            start_time_.tv_sec -= nsec;
        }

        /* Compute the time remaining to wait.
           `tv_nsec' is certainly positive. */
        return static_cast<double>(tspec.tv_sec - start_time_.tv_sec) +
            (static_cast<double>((double)tspec.tv_nsec - (double)start_time_.tv_nsec)
             /    (double)1000000000.0);

    }
    DECLARE_LOGGER("cpu_timer");

    void
    restart()
    {
        int result = clock_gettime(CLOCK_PROCESS_CPUTIME_ID,
                                   &start_time_);
        VERIFY(result == 0);
    }

    double
    resolution() const
    {
        timespec tspec;
        int result = clock_getres(CLOCK_PROCESS_CPUTIME_ID,
                                  &tspec);
        VERIFY(result == 0);
        return tspec.tv_sec + (tspec.tv_nsec / 1000000000.0);
    }

    timespec start_time_;

};
}

#endif // CPU_TIMER

// Local Variables: **
// End: **
