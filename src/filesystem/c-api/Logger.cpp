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

#include "Logger.h"

namespace volumedriverfs
{

void
Logger::ovs_log_init(::youtils::Severity severity)
{
    std::vector<std::string> sinks = {::youtils::Logger::syslog_sink_name()};
    ::youtils::Logger::setupLogging("ovs_logger",
                                    sinks,
                                    severity,
                                    youtils::LogRotation::F);
}

void
Logger::ovs_log_shutdown()
{
    ::youtils::Logger::teardownLogging();
}

void
Logger::ovs_enable_logging()
{
    ::youtils::Logger::enableLogging();
}

void
Logger::ovs_disable_logging()
{
    ::youtils::Logger::disableLogging();
}

::youtils::Logger::logger_type&
Logger::getLogger()
{
    static ::youtils::Logger::logger_type logger("libovsvolumedriver");
    return logger;
}

} //namespace volumedriverfs
