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

#include <youtils/Main.h>
#include <boost/program_options.hpp>
#include "FawltyClient.h"

namespace
{

namespace po = boost::program_options;

uint16_t
port(uint16_t i_port)
{
    static uint16_t port = 0;
    if(port != 0)
    {
        return port;
    }
    else
    {
        port = i_port;
        return 0;
    }
}

uint16_t
port_range(uint16_t i_port_range)
{
    static uint16_t port_range = 0;
    if(port_range != 0)
    {
        return port_range;
    }
    else
    {
        port_range = i_port_range;
        return 0;
    }
}

class FawltyClient : public youtils::TestMainHelper
{
public:
    FawltyClient(int argc,
                 char** argv)
        : TestMainHelper(argc, argv)
        , desc_("Normal options")
    {
        desc_.add_options()
            ("port", po::value<uint16_t>(&port_)->default_value(9090), "port to listen to thrift requests")
            ("port_range", po::value<uint16_t>(&port_range_)->default_value(100), "range of ports to scan");
    }


    virtual void
    setup_logging()
    {
        MainHelper::setup_logging();
    }


    virtual void
    log_extra_help(std::ostream& strm)
    {
        strm << desc_;
        log_google_test_help(strm);
    }

    virtual void
    parse_command_line_arguments()
    {
        init_google_test();

        parse_unparsed_options(desc_,
                               AllowUnregisteredOptions::T,
                               vm_);

        VERIFY(port(port_) == 0);
        VERIFY(port_range(port_range_) == 0);
    }

    po::options_description desc_;
    uint16_t port_;
    uint16_t port_range_;

};

}

MAIN(FawltyClient);
