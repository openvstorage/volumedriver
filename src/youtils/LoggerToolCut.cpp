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
