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
