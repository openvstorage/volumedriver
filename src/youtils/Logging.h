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

#ifndef LOGGING_H_
#define LOGGING_H_

#include "Logger.h"

#define DECLARE_LOGGER(name)                                            \
    static ::youtils::Logger::logger_type&                              \
    getLogger__()                                                       \
    {                                                                   \
        static ::youtils::Logger::logger_type logger(name);             \
        return logger;                                                  \
    }

// LOG_INFO is declared in some /usr/include/syslog.h
#ifndef DONT_DEFINE_LOGGING_MACROS
#include "LoggingMacros.h"
#endif // DONT_DEFINE_LOGGING_MACROS

#endif // LOGGING_H_

// Local Variables: **
// mode : c++ **
// End: **
