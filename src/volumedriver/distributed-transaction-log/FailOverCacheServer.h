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

#ifndef FAILOVERCACHE_SERVER_H_
#define FAILOVERCACHE_SERVER_H_

#include "FailOverCacheAcceptor.h"

#include "../FailOverCacheTransport.h"

#include <signal.h>

#include <cstdlib>
#include <iosfwd>
#include <memory>

#include <boost/program_options.hpp>

#include <youtils/Catchers.h>
#include <youtils/IOException.h>
#include <youtils/Logger.h>
#include <youtils/Logging.h>
#include "fungilib/SocketServer.h"
#include <youtils/Main.h>

namespace distributed_transaction_log_test
{
class FailOverCacheEnvironment;
}

class FailOverCacheServer
    : public youtils::MainHelper
{
    friend class distributed_transaction_log_test::FailOverCacheEnvironment;

public:
    FailOverCacheServer(int argc,
                        char** argv);

    FailOverCacheServer(const constructor_type&);

    virtual void
    log_extra_help(std::ostream& strm);

    void
    parse_command_line_arguments();

    virtual void
    setup_logging();

    int run();

    bool
    running() const;

private:
    static youtils::Logger::logger_type* logger_;

    std::unique_ptr<fungi::SocketServer> s;
    std::unique_ptr<distributed_transaction_log::FailOverCacheAcceptor> acceptor;

    boost::program_options::options_description desc_;

    boost::filesystem::path path_;
    std::string addr_;
    uint16_t port_;
    volumedriver::FailOverCacheTransport transport_;
    bool running_;

    // *ONLY* for testers. Really. I mean it.
    void
    stop_();

    static inline youtils::Logger::logger_type&
    getLogger__()
    {
        ASSERT(logger_);
        return *logger_;
    }
};

#endif // FAILOVERCACHE_SERVER_H_
