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

#include "../Main.h"

#include <string>
#include <iostream>

namespace
{
namespace po = boost::program_options;

struct CompilerLabMain
    : public youtils::TestMainHelper
{
    CompilerLabMain(int argc,
                    char** argv)
        : TestMainHelper(argc,
                         argv)
        , opts_("Tester Specific Options")
    {}

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("compiler_lab");
    }

    virtual void
    log_extra_help(std::ostream& ostr)
    {
        ostr << opts_;
        log_google_test_help(ostr);
    }

    virtual void
    parse_command_line_arguments()
    {
        init_google_test();
    }

private:
    po::options_description opts_;

};
}

MAIN(YoutilsTest)
