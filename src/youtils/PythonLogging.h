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

#ifndef PYTHON_LOGGING_H_
#define PYTHON_LOGGING_H_

#include <boost/python.hpp>
#include "Logger.h"
#include "IOException.h"

#include <string>
#include <boost/filesystem/path.hpp>

namespace pythonyoutils
{
using youtils::Logger;
using youtils::Severity;


class Logging
{
    MAKE_EXCEPTION(LoggingException, fungi::IOException);
    MAKE_EXCEPTION(LoggingAlreadyConfiguredException, LoggingException)
public:
    Logging() = delete;

    Logging&
    operator=(const Logging&) = delete;

    static void
    disableLogging()
    {
        Logger::disableLogging();
    }

    static void
    enableLogging()
    {
        Logger::enableLogging();
    }

    static bool
    loggingEnabled()
    {
        return Logger::loggingEnabled();
    }

    static void
    setupConsoleLogging(Severity severity = Severity::trace,
                        const std::string& progname = "PythonLogger")
    {
        if(loggin_is_setup_)
        {
            throw LoggingAlreadyConfiguredException("Logging already set up");
        }
        else
        {
            const std::vector<std::string> sinks = { Logger::console_sink_name() };
            Logger::setupLogging(progname,
                                 sinks,
                                 severity,
                                 youtils::LogRotation::F);
            loggin_is_setup_ = true;

        }
    }

    static void
    setupFileLogging(const std::string& path,
                     const Severity severity = Severity::trace,
                     const std::string& progname = "PythonLogger")
    {
        if(loggin_is_setup_)
        {
            throw LoggingAlreadyConfiguredException("Logging already set up");
        }
        else
        {
            const std::vector<std::string> sinks = { path };

            Logger::setupLogging(progname,
                                 sinks,
                                 severity,
                                 youtils::LogRotation::F);
            loggin_is_setup_ = true;
        }
    }

    std::string
    str() const
    {
        using namespace std::string_literals;
        return "<Logging>"s;
    }


    std::string
    repr() const
    {
        using namespace std::string_literals;
        return "<Logging>"s;
    }

    static bool loggin_is_setup_ ;

};

}

#endif // PYTHON_LOGGING_H_
