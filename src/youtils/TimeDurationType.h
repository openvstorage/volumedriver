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

#ifndef TIME_DURATION_TYPE_H
#define TIME_DURATION_TYPE_H

#include <boost/date_time/posix_time/posix_time.hpp>

namespace youtils
{
class TimeDurationType
{
public:
    explicit TimeDurationType(const boost::posix_time::time_duration& val)
        : val_(val)
    {}

    operator const boost::posix_time::time_duration& () const
    {
        return val_;
    }
private:
    const boost::posix_time::time_duration val_;
};

}
#define DECLARE_DURATION_TYPE(name)                                     \
    class name : public ::youtils::TimeDurationType                     \
    {                                                                   \
        public:                                                         \
        explicit name(const boost::posix_time::time_duration& val)      \
            : TimeDurationType(val)                                     \
        {}                                                              \
    };

#endif // TIME_DURATION_TYPE_H

// Local Variables: **
// mode: c++ **
// End: **
