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
