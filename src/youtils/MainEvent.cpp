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

#include "MainEvent.h"

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>

namespace youtils
{

MainEvent::MainEvent(const std::string& what,
                     Logger::logger_type& logger)
    : what_(what)
    , logger_(logger)
{
    BOOST_LOG_SEV(logger_.get(),
                  youtils::Severity::notification) << what_ << ", START" ;
}

MainEvent::~MainEvent()
{
    if(std::uncaught_exception())
    {
        try
        {
            BOOST_LOG_SEV(logger_.get(),
                          youtils::Severity::notification) << what_ << ", EXCEPTION";
        }
        catch(...)
        {}

    }
    else
    {
        BOOST_LOG_SEV(logger_.get(),
                      youtils::Severity::notification) << what_  << ", FINISHED";
    }
}

}
