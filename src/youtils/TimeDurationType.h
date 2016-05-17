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
