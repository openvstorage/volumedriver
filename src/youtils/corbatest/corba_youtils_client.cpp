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

#include "../TestMainHelper.h"
#include "../Main.h"

namespace youtils_test
{
using namespace youtils;
namespace po = boost::program_options;

class CorbaYoutilsClient : public TestMainHelper
{
public:
    CorbaYoutilsClient(int argc,
                       char** argv)
        : TestMainHelper(argc,
                         argv)
        , opts_("Tester Specific Options")
    {}

    void
    parse_command_line_arguments() override final
    {
        init_google_test();
        parse_unparsed_options(opts_,
                               youtils::AllowUnregisteredOptions::F,
                               vm_);
    }

    void
    log_extra_help(std::ostream& strm) override final
    {
        strm << opts_;
        log_google_test_help(strm);
    }

    void
    setup_logging() override final
    {
        MainHelper::setup_logging("corba_youtils_client");
    }

private:
    po::options_description opts_;
};
}

MAIN(youtils_test::CorbaYoutilsClient)
