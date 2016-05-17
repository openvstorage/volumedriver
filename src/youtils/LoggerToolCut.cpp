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

#include "LoggerToolCut.h"

#include <boost/filesystem.hpp>

namespace toolcut
{

using namespace youtils;

bool
LoggerToolCut::isLoggingEnabled()
{
    return Logger::loggingEnabled();
}

void
LoggerToolCut::enableLogging()
{
    Logger::enableLogging();
}

void
LoggerToolCut::disableLogging()
{
    Logger::disableLogging();
}

youtils::Severity
LoggerToolCut::getGeneralLoggingLevel()
{
    return Logger::generalLogging();
}

void
LoggerToolCut::setGeneralLoggingLevel(youtils::Severity sev)
{
    Logger::generalLogging(sev);
}

void
LoggerToolCut::setupLogging(const std::string& file,
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

void
LoggerToolCut::teardownLogging()
{
    Logger::teardownLogging();
}

boost::python::dict
LoggerToolCut::listFilters()
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

void
LoggerToolCut::addFilter(const std::string& name,
                         const youtils::Severity severity)
{
    Logger::add_filter(name, severity);
}

void
LoggerToolCut::removeFilter(const std::string& name)
{
    Logger::remove_filter(name);
}

void
LoggerToolCut::removeAllFilters()
{
    Logger::remove_all_filters();
}

}
