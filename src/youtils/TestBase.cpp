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
