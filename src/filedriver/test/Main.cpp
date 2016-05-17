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
        MainHelper::setup_logging("filedriver_test");
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
