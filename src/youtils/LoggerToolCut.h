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
                 const youtils::LogRotation,
                 const std::string& = "LoggerToolCut");

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
