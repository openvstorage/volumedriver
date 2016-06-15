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

#include "../ArakoonTestSetup.h"
#include "../Main.h"

#include <string>
#include <iostream>

namespace
{
namespace po = boost::program_options;

struct YoutilsTest
    : public youtils::TestMainHelper
{
    YoutilsTest(int argc,
                char** argv)
        : TestMainHelper(argc,
                     argv)
        , opts_("Tester Specific Options")
    {
        opts_.add_options()
            ("arakoon-binary-path",
             po::value<std::string>(&arakoon_binary_path_)->default_value("/usr/bin/arakoon"),
             "path to arakoon binary")
            ("arakoon-port-base",
             po::value<uint16_t>(&arakoon_port_base_)->default_value(12345),
             "arakoon port base");
    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("youtils_test");
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

        parse_unparsed_options(opts_,
                               youtils::AllowUnregisteredOptions::F,
                               vm_);

        arakoon::ArakoonTestSetup::setArakoonBinaryPath(arakoon_binary_path_);
        arakoon::ArakoonTestSetup::setArakoonBasePort(arakoon_port_base_);
    }

private:
    po::options_description opts_;
    std::string arakoon_binary_path_;
    uint16_t arakoon_port_base_;
};
}

MAIN(YoutilsTest)


// Local Variables: **
// mode: c++ **
// End: **
