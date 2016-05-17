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

#include "Assert.h"
#include "Logger.h"
#include "RocksLogger.h"

namespace youtils
{

namespace rdb = rocksdb;

RocksLogger::RocksLogger(const std::string& id)
    : id_(id)
{}

Severity
RocksLogger::translate_level_(rdb::InfoLogLevel lvl)
{
    switch (lvl)
    {
    case rdb::InfoLogLevel::DEBUG_LEVEL:
        return Severity::debug;
    case rdb::InfoLogLevel::INFO_LEVEL:
        return Severity::info;
    case rdb::InfoLogLevel::WARN_LEVEL:
        return Severity::warning;
    case rdb::InfoLogLevel::ERROR_LEVEL:
        return Severity::error;
    case rdb::InfoLogLevel::FATAL_LEVEL:
        return Severity::fatal;
    default:
        LOG_ERROR("Unknown log level " << static_cast<uint64_t>(lvl));
        ASSERT(false);
        return Severity::error;
    }
}

void
RocksLogger::Logv(const char*,
                  va_list)
{
    ASSERT(false);
}

void
RocksLogger::Logv(const rocksdb::InfoLogLevel log_level,
                  const char* fmt,
                  va_list ap)
{
    const Severity sev(translate_level_(log_level));

    if (youtils::Logger::filter(getLogger__().name,
                                sev))
    {
        char buf[512];
        vsnprintf(buf,
                  sizeof(buf) - 1,
                  fmt,
                  ap);

        BOOST_LOG_SEV(getLogger__().get(), sev) << id_ << ": " << buf;
    }
}

}
