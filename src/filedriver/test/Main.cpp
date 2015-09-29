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

#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/ptr_container/ptr_map.hpp>

#include <google/protobuf/stubs/common.h>

#include <youtils/BuildInfoString.h>
#include <youtils/Logger.h>
#include <youtils/Main.h>
#include <youtils/ProtobufLogger.h>
#include <youtils/TestMainHelper.h>

#include <backend/BackendTestSetup.h>
#include <backend/TestWithBackendMainHelper.h>
namespace
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace yt = youtils;

class FileDriverTest
    : public backend::TestWithBackendMainHelper
{
public:
    FileDriverTest(int argc,
                   char** argv)
        : TestWithBackendMainHelper(argc,
                                    argv)
        , desc_("FileDriverTest Options")
    {}

    virtual void
    log_extra_help(std::ostream& os)
    {
        log_backend_setup_help(os);
        log_google_test_help(os);
    }

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging();
        yt::ProtobufLogger::setup();
    }

    virtual void
    parse_command_line_arguments()
    {
        init_google_test();

        TestWithBackendMainHelper::parse_command_line_arguments();

        parse_unparsed_options(desc_,
                               yt::AllowUnregisteredOptions::T,
                               vm_);
    }


private:
    po::options_description desc_;
};

}

MAIN(FileDriverTest);
