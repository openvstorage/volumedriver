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
