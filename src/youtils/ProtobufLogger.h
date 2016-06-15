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

#ifndef YOUTILS_PROTOBUF_LOGGER_H_
#define YOUTILS_PROTOBUF_LOGGER_H_

#include "Logging.h"

#include <google/protobuf/stubs/common.h>

namespace youtils
{

struct ProtobufLogger
{
    static void
    log(google::protobuf::LogLevel level,
        const char* filename,
        int line,
        const std::string& message)
    {
#define MSG \
        filename << "(" << line << "): " << message

        switch (level)
        {
        case google::protobuf::LogLevel::LOGLEVEL_INFO:
            LOG_INFO(MSG);
            break;
        case google::protobuf::LogLevel::LOGLEVEL_ERROR:
            LOG_ERROR(MSG);
            break;
        case google::protobuf::LogLevel::LOGLEVEL_FATAL:
            LOG_FATAL(MSG);
            break;
        case google::protobuf::LogLevel::LOGLEVEL_WARNING:
            LOG_WARN(MSG);
            break;
        }
#undef MSG
    }

    static void
    setup()
    {
        google::protobuf::SetLogHandler(&ProtobufLogger::log);
    }

private:
    DECLARE_LOGGER("ProtobufLogger");
};

}

#endif // !YOUTILS_PROTOBUF_LOGGER_H_
