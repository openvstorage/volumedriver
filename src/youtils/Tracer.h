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

#ifndef YOUTILS_TRACER_H_
#define YOUTILS_TRACER_H_

#include "Logger.h"
#include "Logging.h"

namespace youtils
{

class Tracer
{
public:
    Tracer(Logger::logger_type& l,
           const std::string& s)
        : l_(l)
        , s_(s)
    {
        BOOST_LOG_SEV(l_.get(), youtils::Severity::trace) << "Enter: " << s_;
    }

    ~Tracer()
    {
        BOOST_LOG_SEV(l_.get(), youtils::Severity::trace) << "Exit: " << s_;
    }

    Logger::logger_type& l_;
    const std::string s_;
};

#ifndef NDEBUG
#define TRACER ::youtils::Tracer _t(getLogger__(), __FUNCTION__)
#else
#define TRACER
#endif

}

#endif // !YOUTILS_TRACER_H_

// Local Variables: **
// mode: c++ **
// End: **
