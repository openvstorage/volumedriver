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

#include <youtils/Logger.h>
#include <boost/type_index.hpp>

namespace volumedriverfs
{

class Logger
{
public:
    static void
    ovs_log_init(::youtils::Severity);

    static void
    ovs_log_shutdown();

    static void
    ovs_enable_logging();

    static void
    ovs_disable_logging();

    static ::youtils::Logger::logger_type&
    getLogger();
};

} //namespace volumedriverfs

#define LIBLOG_(sev, msg)                                                     \
    if (::youtils::Logger::filter(volumedriverfs::Logger::getLogger().name,   \
                                  sev))                                       \
    {                                                                         \
        BOOST_LOG_SEV(volumedriverfs::Logger::getLogger().get(), sev) <<      \
        __FUNCTION__ << "(" << __LINE__ << "): " << msg;                      \
    }

#define LIBLOG_ID_(sev, msg)                                                  \
    if (::youtils::Logger::filter(volumedriverfs::Logger::getLogger().name,   \
                                  sev))                                       \
    {                                                                         \
        BOOST_LOG_SEV(volumedriverfs::Logger::getLogger().get(), sev) <<      \
        boost::typeindex::type_id_runtime(*this).pretty_name() <<             \
        "(" << this << ")" << " - " <<                                        \
        __FUNCTION__ << "(" << __LINE__ << "): " << msg;                      \
    }

#define LIBLOG_TRACE(msg)  LIBLOG_(::youtils::Severity::trace, msg)
#define LIBLOG_DEBUG(msg)  LIBLOG_(::youtils::Severity::debug, msg)
#define LIBLOG_INFO(msg)   LIBLOG_(::youtils::Severity::info, msg)
#define LIBLOG_WARN(msg)   LIBLOG_(::youtils::Severity::warn, msg)
#define LIBLOG_ERROR(msg)  LIBLOG_(::youtils::Severity::error, msg)
#define LIBLOG_FATAL(msg)  LIBLOG_(::youtils::Severity::fatal, msg)
#define LIBLOG_NOTIFY(msg) LIBLOG_(::youtils::Severity::notification, msg)

#define LIBLOGID_TRACE(msg)  LIBLOG_ID_(::youtils::Severity::trace, msg)
#define LIBLOGID_DEBUG(msg)  LIBLOG_ID_(::youtils::Severity::debug, msg)
#define LIBLOGID_INFO(msg)   LIBLOG_ID_(::youtils::Severity::info, msg)
#define LIBLOGID_WARN(msg)   LIBLOG_ID_(::youtils::Severity::warn, msg)
#define LIBLOGID_ERROR(msg)  LIBLOG_ID_(::youtils::Severity::error, msg)
#define LIBLOGID_FATAL(msg)  LIBLOG_ID_(::youtils::Severity::fatal, msg)
#define LIBLOGID_NOTIFY(msg) LIBLOG_ID_(::youtils::Severity::notification, msg)
