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

#include "LoggingToolCut.h"

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>

namespace toolcut
{
void
LoggingToolCut::operator()(const youtils::Severity sev,
                           const std::string& str)
{
    if (youtils::Logger::filter(severity_logger->name,
                                sev))
    {
        BOOST_LOG_SEV(severity_logger->get(), sev) << str;
    }
}

const std::string&
LoggingToolCut::name() const
{
    return name_;
}

}
