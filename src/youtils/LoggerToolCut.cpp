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
                            const LogRotation log_rotation)
{
    Logger::setupLogging(file,
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
