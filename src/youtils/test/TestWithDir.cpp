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
    TestBase::SetUp();
    fs::remove_all(directory_);
    fs::create_directories(directory_);
}

void TestWithDir::TearDown()
{
    //no removal of the dir on teardown for debugging purposes, only on next Setup
    TestBase::TearDown();
}
}

// Local Variables: **
// bvirtual-targets: ("target/bin/youtils_test") **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
