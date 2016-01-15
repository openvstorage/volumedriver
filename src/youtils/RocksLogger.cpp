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
