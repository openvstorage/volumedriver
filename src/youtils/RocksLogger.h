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

#ifndef YT_ROCKS_LOGGER_H_
#define YT_ROCKS_LOGGER_H_

#include "Logging.h"

#include <rocksdb/env.h>

namespace youtils
{

class RocksLogger
    : public rocksdb::Logger
{
public:
    explicit RocksLogger(const std::string& id);

    virtual ~RocksLogger() = default;

    RocksLogger(const RocksLogger&) = delete;

    RocksLogger&
    operator=(const RocksLogger&) = delete;

    virtual void
    Logv(const char* fmt,
         va_list ap) override final;

    virtual void
    Logv(const rocksdb::InfoLogLevel log_level,
         const char* fmt,
         va_list ap) override final;

private:
    DECLARE_LOGGER("RocksLogger");

    const std::string id_;

    static Severity
    translate_level_(rocksdb::InfoLogLevel lvl);

};

}

#endif // !YT_ROCKS_LOGGER_H_
