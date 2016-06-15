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

#ifndef _LOGGING_TOOLCUT_H
#define _LOGGING_TOOLCUT_H
#include "Logger.h"

namespace toolcut
{
class LoggingToolCut
{
public:
    LoggingToolCut(const std::string& name)
        :severity_logger(new youtils::SeverityLoggerWithName(name))
        , name_(name)
    {}

    void
    operator()(const youtils::Severity,
               const std::string&);

    const std::string&
    name() const;

private:
    std::shared_ptr<youtils::SeverityLoggerWithName> severity_logger;
    const std::string name_;
};

}

#endif // _LOGGING_TOOLCUT_H
