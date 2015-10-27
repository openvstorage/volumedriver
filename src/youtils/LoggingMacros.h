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


#define LOG_DEBUG(message) _LOG_SEV(youtils::Severity::debug, message)

#define LOG_TRACE(message) _LOG_SEV(youtils::Severity::trace, message)

#endif // !LOGGING_MACROS_H_
