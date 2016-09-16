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

#include <gtest/gtest.h>
#include "../MainHelper.h"
#include "../TestMainHelper.h"


namespace youtilstest
{
namespace po = boost::program_options;

struct MainHelperTestExecutable
    : public youtils::MainHelper
{
    MainHelperTestExecutable(int argc,
                             char** argv)
        : MainHelper(argc, argv)
    {}

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("main_helper_test_executable");
    }

    void
    log_extra_help(std::ostream&)
    {}

    void
    parse_command_line_arguments()
    {}

    int
    run()
    {
        return 0;
    }
};

struct TestMainHelperTestExecutable
    : public youtils::TestMainHelper
{
    TestMainHelperTestExecutable(int argc,
                                 char** argv)
        : TestMainHelper(argc, argv)
    {
        normal_options_.add_options()
            ("arg1",
             po::value<std::string>(&arg1_)->default_value(""),
             "arg1, a string")
            ("arg2",
             po::value<unsigned>(&arg2_)->default_value(23096),
             "arg2, an unsigned")
            ("namespace",
             po::value<bool>(&arg3_)->default_value(true),
             "arg3, a boolean");
    }

    virtual void
    setup_logging()
    {
        youtils::MainHelper::setup_logging("main_helper_test");
    }

    virtual void
    log_extra_help(std::ostream& ostr)
    {
        ostr << normal_options_;
        log_google_test_help(ostr);
        for(unsigned i = 0; i < 100; ++i)
        {
            LOG_INFO("Running the tester");
        }
    }

    virtual void
    parse_command_line_arguments()
    {
        init_google_test();
        parse_unparsed_options(normal_options_,
                               youtils::AllowUnregisteredOptions::T,
                               vm_);
    }

    po::options_description normal_options_;
    std::string arg1_;
    unsigned arg2_;
    bool arg3_;
};

// The
class MainHelperTest : public testing::Test
{

};

}
