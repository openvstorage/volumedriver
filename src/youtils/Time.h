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
