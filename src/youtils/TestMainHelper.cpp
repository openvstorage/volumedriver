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

#include "TestMainHelper.h"
#include <signal.h>
#include <gtest/gtest.h>

namespace youtils
{

void
TestMainHelper::sighand(int)
{
    return;
}

TestMainHelper::TestMainHelper(int argc,
                               char** argv)
    : MainHelper(argc,
                 argv)
{
/*    if(geteuid() == 0)
    {
        std::string msg("These tests cannot be run as root");
        LOG_FATAL(msg);
        throw fungi::IOException(msg.c_str());
    }*/

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, 0);
    sa.sa_handler = &sighand;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, 0);
}

void
TestMainHelper::log_google_test_help(std::ostream& ostr)
{
    // redirecting cout so google help it goes to the correct stream
    const char* argv[] =
        {
            executable_name_.c_str(),
            "--help"
        };
    int size = 2;

    std::streambuf* old_rdbuf = std::cout.rdbuf();
    try
    {
        std::cout.rdbuf(ostr.rdbuf());
        testing::InitGoogleTest(&size,
                                (char**)argv);
    }
    catch(...)
    {
        std::cout.rdbuf(old_rdbuf);
        throw;
    }
    std::cout.rdbuf(old_rdbuf);
}

void
TestMainHelper::init_google_test()
{

    ArgcArgv args(executable_name_.c_str(),
                  unparsed_options());

    testing::InitGoogleTest(args.argc(), args.argv());
    unparsed_options(*args.argc(), args.argv());
}

int
TestMainHelper::run()
{
    return RUN_ALL_TESTS();
}

}
