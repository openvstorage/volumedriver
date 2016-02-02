// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LOGGER_H_
#define LOGGER_H_

#include "BooleanEnum.h"
#include "IOException.h"

#include <boost/log/sources/severity_logger.hpp>

namespace boost
{
namespace filesystem
{

class path;

}

}

namespace youtils
{

BOOLEAN_ENUM(LogRotation);

enum class Severity
{
    trace,
    debug,
    periodic,
    info,
    warning,
    error,
    fatal,
    notification
};

std::ostream&
operator<<(std::ostream& os,
           Severity severity);

std::istream&
operator>>(std::istream& is,
           Severity& severity);

MAKE_EXCEPTION(LoggerException, fungi::IOException);
MAKE_EXCEPTION(LoggerNotConfiguredException, LoggerException);
MAKE_EXCEPTION(LoggerAlreadyConfiguredException, LoggerException);

class SeverityLoggerWithName
{
public:
    using logger_type = boost::log::sources::severity_logger_mt<Severity>;

    explicit SeverityLoggerWithName(const std::string& n,
                                    const std::string& subsystem = "no subsystem");

    SeverityLoggerWithName(const SeverityLoggerWithName&) = delete;

    SeverityLoggerWithName&
    operator=(const SeverityLoggerWithName&) = delete;

    inline logger_type&
    get()
    {
        return logger_;
    }

    // We store it here as well as extracting it from logger_ incurs extra overhead at
    // runtime.
    const std::string name;

private:
    logger_type logger_;
};

class Logger
{
public:
    typedef std::pair<std::string, youtils::Severity> filter_t;
    typedef SeverityLoggerWithName logger_type;

    Logger() = default;

    ~Logger() = default;

    static const std::string&
    console_sink_name()
    {
        static const std::string n("console:");
        return n;
    }

    static void
    disableLogging();

    static void
    enableLogging();

    static bool
    loggingEnabled();

    static void
    setupLogging(const std::string& progname,
                 const std::vector<std::string>& sinks,
                 Severity severity,
                 const LogRotation);

    static void
    teardownLogging();

    static void
    generalLogging(Severity severity);

    static youtils::Severity
    generalLogging();

    static void
    add_filter(const std::string& filter_name,
               const Severity severity);

    static void
    remove_filter(const std::string& filter_name);

    static void
    remove_all_filters();

    static void
    all_filters(std::vector<filter_t>& out);

    static bool
    filter(const std::string& name,
           const Severity sev);

    // Helper functions for debugging
#define LOG_HELPER(x)                                           \
    static void                                                 \
    x##_log(const char* in)  __attribute__((used))              \
    {                                                           \
        add_filter(in,                                          \
                   Severity::x);                                \
    }                                                           \

    LOG_HELPER(trace);
    LOG_HELPER(debug);
    LOG_HELPER(periodic);
    LOG_HELPER(info);
    LOG_HELPER(warning);
    LOG_HELPER(error);
    LOG_HELPER(fatal);
    LOG_HELPER(notification)

#undef LOG_HELPER

    static void
    log_clear(const char* in) __attribute__((used))
    {
        remove_filter(in);
    }
};

}

#endif // LOGGER_H_
