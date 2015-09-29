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

#include "../TestBase.h"
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
        MainHelper::setup_logging();
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
        youtils::MainHelper::setup_logging();
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
class MainHelperTest : public TestBase
{

};

}
