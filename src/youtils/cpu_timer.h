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
