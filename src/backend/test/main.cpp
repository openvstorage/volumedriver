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
