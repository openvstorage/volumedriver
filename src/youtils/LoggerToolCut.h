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

#ifndef _LOGGER_TOOLCUT_H_
#define _LOGGER_TOOLCUT_H_
#include "Logger.h"
#include <boost/python/dict.hpp>

namespace toolcut
{

class LoggerToolCut
{
private:
    LoggerToolCut() = delete;
    ~LoggerToolCut() = delete;
public:

    static bool
    isLoggingEnabled();

    static youtils::Severity
    getGeneralLoggingLevel();

    static void
    setGeneralLoggingLevel(youtils::Severity);

    static void
    setupLogging(const std::string&,
                 const youtils::Severity,
                 const youtils::LogRotation);

    static void
    teardownLogging();

    static void
    enableLogging();

    static void
    disableLogging();

    static boost::python::dict
    listFilters();

    static void
    addFilter(const std::string&,
              const youtils::Severity);

    static void
    removeFilter(const std::string&);

    static void
    removeAllFilters();
};

}

#endif // _LOGGER_TOOLCUT_H_
