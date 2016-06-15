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
namespace
{
namespace po = boost::program_options;

struct FawltyFSTest : public youtils::TestMainHelper
{
    FawltyFSTest(int argc,
                char** argv)
        : TestMainHelper(argc,
                         argv)
    {}

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("fawlty_fs_test");
    }

    virtual void
    log_extra_help(std::ostream& ostr)
    {
        log_google_test_help(ostr);
    }

    virtual void
    parse_command_line_arguments()
    {
        init_google_test();
    }
};
}

MAIN(FawltyFSTest)

// Local Variables: **
// mode: c++ **
// End: **
