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

#ifndef TIME_H_
#define TIME_H_

#include <boost/date_time/posix_time/posix_time.hpp>

struct Time
{
    static inline std::string
    getCurrentTimeAsString()
    {
        return boost::posix_time::to_simple_string(boost::posix_time::second_clock::universal_time());
    }

    static inline boost::posix_time::ptime
    timeFromString(const std::string& str)
    {
        return boost::posix_time::time_from_string(str);
    }

};

#endif // TIME_H_

// Local Variables: **
// End: **
