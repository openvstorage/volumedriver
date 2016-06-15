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

#include "TestBase.h"
#include "FileUtils.h"

namespace youtilstest
{
using namespace youtils;

void
TestBase::Run()
{
    try
    {
        testing::Test::Run();
    }
    catch (fungi::IOException &e)
    {
        TearDown();
        FAIL() << "IOException: " << e.what();
    }
    catch (std::exception &e)
    {
        TearDown();
        FAIL() << "exception: " << e.what();
    }
    catch (...)
    {
        TearDown();
        FAIL() << "unknown exception";
    }
}

fs::path
TestBase::getTempPath(const std::string& ipath)
{
    fs::path tmp_path = FileUtils::temp_path();
    fs::create_directories(tmp_path);
    return tmp_path / ipath;
}


}

// Local Variables: **
// mode: c++ **
// End: **
