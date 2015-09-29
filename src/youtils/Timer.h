// Copyright 2015 Open vStorage NV
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
