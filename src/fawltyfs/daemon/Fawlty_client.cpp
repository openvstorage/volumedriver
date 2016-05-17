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
        MainHelper::setup_logging("fawltyfs_client");
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
