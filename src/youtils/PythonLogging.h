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
    setupConsoleLogging(Severity severity = Severity::trace)
    {
        if(loggin_is_setup_)
        {
            throw LoggingAlreadyConfiguredException("Loggin already set up");
        }
        else
        {

            Logger::setupLogging("",
                                 severity,
                                 youtils::LogRotation::F);
            loggin_is_setup_ = true;

        }
    }

    static void
    setupFileLogging(const std::string& path,
                     const Severity severity = Severity::trace)
    {
        if(loggin_is_setup_)
        {
            throw LoggingAlreadyConfiguredException("Loggin already set up");
        }
        else
        {

            Logger::setupLogging(path,
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
