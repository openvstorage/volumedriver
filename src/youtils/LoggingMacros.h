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

#ifndef LOGGING_MACROS_H_
#define LOGGING_MACROS_H_

#include "Logger.h"

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>

#define _LOG_SEV(sev, message)                                          \
    if (youtils::Logger::filter(getLogger__().name,                     \
                                sev))                                   \
    {                                                                   \
        BOOST_LOG_SEV(getLogger__().get(), sev) << __FUNCTION__  << ": " << message; \
    }

#define LOG_NOTIFY(message) _LOG_SEV(youtils::Severity::notification, message)

#define LOG_FATAL(message) _LOG_SEV(youtils::Severity::fatal, message)

#define LOG_ERROR(message) _LOG_SEV(youtils::Severity::error, message)

#define LOG_WARN(message) _LOG_SEV(youtils::Severity::warning, message)


// FOR FUCKING SYSLOG -> not a really nice solution I know
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#define LOG_INFO(message) _LOG_SEV(youtils::Severity::info, message)

//
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif

#define LOG_PERIODIC(message) _LOG_SEV(youtils::Severity::periodic, message)


#define LOG_DEBUG(message)

#define LOG_TRACE(message)

#endif // !LOGGING_MACROS_H_
