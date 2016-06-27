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

#ifndef DISTRIBUTED_TRANSACTION_LOG_TEST_MAIN_H_
#define DISTRIBUTED_TRANSACTION_LOG_TEST_MAIN_H_

#include <string>

#include <youtils/Main.h>
#include <backend/Namespace.h>

#include <volumedriver/FailOverCacheConfig.h>
#include <volumedriver/FailOverCacheTransport.h>

class DtlTestMain
    : public youtils::TestMainHelper
{
public:
    DtlTestMain(int argc,
                          char** argv);

    virtual void
    setup_logging();

    virtual void
    log_extra_help(std::ostream& ostr);

    virtual void
    parse_command_line_arguments();

    static volumedriver::FailOverCacheConfig
    failovercache_config()
    {
        using namespace std::literals::string_literals;
        return volumedriver::FailOverCacheConfig(host() ? *host() : "127.0.0.1"s,
                                                 port(),
                                                 volumedriver::FailOverCacheMode::Asynchronous);
    }

    static const boost::optional<std::string>
    host()
    {
        if (host_.empty())
        {
            return boost::none;
        }
        else
        {
            return host_;
        }
    }

    static uint16_t
    port()
    {
        return port_;
    }

    static volumedriver::FailOverCacheTransport
    transport()
    {
        return transport_;
    }

    static const backend::Namespace&
    ns()
    {
        return *ns_;
    }

private:
    boost::program_options::options_description normal_options_;
    static std::string host_;
    static uint16_t port_;
    static volumedriver::FailOverCacheTransport transport_;
    std::string ns_temp_;

    static std::unique_ptr<backend::Namespace> ns_;
};
#endif // DISTRIBUTED_TRANSACTION_LOG_TEST_MAIN_H_
