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

#include "../BackendTestSetup.h"

#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/ptr_container/ptr_map.hpp>

#include <youtils/BuildInfo.h>
#include "youtils/TestBase.h"
#include <youtils/Logger.h>

#include <../BackendParameters.h>
#include "../TestWithBackendMainHelper.h"
#include <youtils/Main.h>


namespace yt = youtils;
namespace be = backend;
namespace fs = boost::filesystem;
namespace po = boost::program_options;

struct BackendTest : public backend::TestWithBackendMainHelper
{
    BackendTest(int argc,
                char** argv)
        : TestWithBackendMainHelper(argc,
                                    argv)
    {}

    virtual void
    setup_logging()
    {
        MainHelper::setup_logging("backend_test");
    }

    virtual void
    log_extra_help(std::ostream& strm)
    {
        log_backend_setup_help(strm);

        log_google_test_help(strm);
    }

};

MAIN(BackendTest)

// Local Variables: **
// mode: c++ **
// End: **
