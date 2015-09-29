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
