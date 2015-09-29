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

    static void
    disableLogging();

    static void
    enableLogging();

    static bool
    loggingEnabled();

    static void
    setupLogging(const boost::filesystem::path& log_file_name,
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
