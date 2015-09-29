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

namespace failovercachetest
{
class FailOverCacheEnvironment;
}

class FailOverCacheServer
    : public youtils::MainHelper
{
    friend class failovercachetest::FailOverCacheEnvironment;

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
    std::unique_ptr<failovercache::FailOverCacheAcceptor> acceptor;

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
