// Copyright (C) 2017 iNuron NV
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

#include "LoggingAdapter.h"

#include <vector>

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/python.hpp>
#include <boost/python/class.hpp>
#include <boost/python/enum.hpp>

#include <youtils/Logger.h>

// "Logger" vs. "Logging" seems to be backwards here:
// "Logger" (an entity that logs?) actually configures the logging subsystem
// (enable, disable, filtering, ...) whereas "Logging" (the logging subsystem?)
// is used here as a term for an individual entity that logs.
// We keep it that way for to avoid API breakage on the python side.
namespace youtils
{

namespace python
{

namespace bpy = boost::python;

namespace
{

Severity
get_general_logging_level()
{
    return Logger::generalLogging();
}

void
set_general_logging_level(Severity sev)
{
    Logger::generalLogging(sev);
}

void
setup_logging(const std::string& file,
              const Severity severity,
              const LogRotation log_rotation,
              const std::string& progname)
{
    std::vector<std::string> vec;

    if (not file.empty())
    {
        vec.push_back(file);
    }
    else
    {
        vec.push_back(youtils::Logger::console_sink_name());
    }

    Logger::setupLogging(progname,
                         vec,
                         severity,
                         log_rotation);
}

boost::python::dict
list_filters()
{
    std::vector<Logger::filter_t> filters;

    Logger::all_filters(filters);
    boost::python::dict dct;
    for(Logger::filter_t& filter : filters)
    {
        dct[filter.first] = filter.second;
    }
    return dct;
}

class LoggerCallable
{
public:
    explicit LoggerCallable(const std::string& name)
        :severity_logger(new youtils::SeverityLoggerWithName(name))
        , name_(name)
    {}

    void
    operator()(const Severity sev,
               const std::string& str)
    {
        if (Logger::filter(severity_logger->name,
                           sev))
        {
            BOOST_LOG_SEV(severity_logger->get(), sev) << str;
        }
    }

    const std::string&
    name() const
    {
        return name_;
    }

private:
    std::shared_ptr<youtils::SeverityLoggerWithName> severity_logger;
    const std::string name_;
};

}

DEFINE_PYTHON_WRAPPER(LoggingAdapter)
{
    bpy::enum_<Severity>("Severity",
                         "Severity levels of logging\n"
                         "Values are trace, debug, info, warning, error, fatal and notification")
        .value("trace", Severity::trace)
        .value("debug", Severity::debug)
        .value("periodic", Severity::periodic)
        .value("info", Severity::info)
        .value("warning", Severity::warning)
        .value("error", Severity::error)
        .value("fatal", Severity::fatal)
        .value("notification", Severity::notification);


    bpy::enum_<LogRotation>("LogRotationType",
                                "Boolean enum, whether to rotate the log")
        .value("T", LogRotation::T)
        .value("F", LogRotation::F);

    bpy::class_<Logger,
                boost::noncopyable>("Logger",
                                    "Allows logger configuration",
                                    bpy::no_init)
        .def("getGeneralLoggingLevel",
             &get_general_logging_level,
             "Get the general logging level",
             "@result an SeverityType enum or an None if logging is disabled")
        .staticmethod("getGeneralLoggingLevel")
        .def("setGeneralLoggingLevel",
             &set_general_logging_level,
             "set the general logging level"
             "@param A SeverityType enum")
        .staticmethod("setGeneralLoggingLevel")
        .def("setupLogging",
             &setup_logging,
             (bpy::args("LogFile") = std::string(""),
              bpy::args("SeverityLevel") = Severity::info,
              bpy::args("LogRotation") = LogRotation::F,
              bpy::args("ProgName") = std::string("LoggerToolCut")),
             "setup the logging\n"
             "@param filename to log to, a string\n"
             "@param severity to log at, a SeverityType\n"
             "@param rotate the log, a LogRotationType")
        .staticmethod("setupLogging")
        .def("teardownLogging",
             &Logger::teardownLogging,
             "teardown the logging")
        .staticmethod("teardownLogging")
        .def("listFilters",
             &list_filters,
             "returns a list of all the filters")
        .staticmethod("listFilters")
        .def("addFilter",
             &Logger::add_filter,
             "add a filter to the Logging\n"
             "@param the logger to configure, a string\n"
             "@param severity to log at, a SeverityType\n")
        .staticmethod("addFilter")
        .def("removeFilter",
             &Logger::remove_filter,
             "remove a filter from the logging\n"
             "@param logger to remove, a string\n")
        .staticmethod("removeFilter")
        .def("removeAllFilters",
             &Logger::remove_all_filters,
             "remove all filters from the logging\n")
        .staticmethod("removeAllFilters")
        .def("loggingEnabled",
             &Logger::loggingEnabled,
             "@returns a boolean indicating whether logging is enabled.")
        .staticmethod("loggingEnabled")
        .def("enableLogging",
             &Logger::enableLogging,
             "enable the logging")
        .staticmethod("enableLogging")
        .def("disableLogging",
             &Logger::disableLogging,
             "disable the logging")
        .staticmethod("disableLogging")
        ;

    bpy::class_<LoggerCallable>("Logging",
                                "A Logger class",
                                bpy::init<const std::string>("Creates a logger with the specified name"))
        .def("__call__",
             &LoggerCallable::operator(),
             (bpy::args("Severity") = Severity::info,
              bpy::args("Message")= ""),
             "Log a message with the given severity\n"
             "@param a log level, type SeverityType\n"
             "@param a message, type string\n")
        ;
}

}

}

// Local Variables: **
// mode: c++ **
// End: **
