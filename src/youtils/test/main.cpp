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
        MainHelper::setup_logging();
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
