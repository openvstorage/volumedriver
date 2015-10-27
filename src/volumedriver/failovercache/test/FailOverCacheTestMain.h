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

#ifndef FAILOVERCACHE_TEST_MAIN_H_
#define FAILOVERCACHE_TEST_MAIN_H_

#include <string>

#include <youtils/Main.h>
#include <backend/Namespace.h>

#include <volumedriver/FailOverCacheConfig.h>
#include <volumedriver/FailOverCacheTransport.h>

class FailOverCacheTestMain
    : public youtils::TestMainHelper
{
public:

    FailOverCacheTestMain(int argc,
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
                                                 mode());
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

    static volumedriver::FailOverCacheMode
    mode()
    {
        return mode_;
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
    static volumedriver::FailOverCacheMode mode_;
    static volumedriver::FailOverCacheTransport transport_;
    std::string ns_temp_;

    static std::unique_ptr<backend::Namespace> ns_;
};
#endif // FAILOVERCACHE_TEST_MAIN_H_
