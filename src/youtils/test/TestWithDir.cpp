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

#include "TestWithDir.h"
#include "../FileUtils.h"
namespace youtilstest
{
using namespace youtils;

TestWithDir::TestWithDir(const std::string& dirName) :
        directory_(FileUtils::temp_path() / dirName)
{}

TestWithDir::TestWithDir() :
        directory_(FileUtils::temp_path() / "testWithDir")
{}

const fs::path& TestWithDir::getDir()
{
    return directory_;
}

void TestWithDir::SetUp()
{
    testing::Test::SetUp();
    fs::remove_all(directory_);
    fs::create_directories(directory_);
}

void TestWithDir::TearDown()
{
    //no removal of the dir on teardown for debugging purposes, only on next Setup
    testing::Test::TearDown();
}

}

// Local Variables: **
// mode: c++ **
// End: **
