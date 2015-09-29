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

#include "../TestBase.h"
#include "../System.h"

namespace youtilstest
{
using namespace youtils;

class SystemTest
    : public TestBase
{};

TEST_F(SystemTest, DISABLED_test_data)
{
    std::vector<int> vec;
    int* p = vec.data();
    for(auto i: vec)
    {
        *p = i;
    }
}

TEST_F(SystemTest, test_hostname)
{
    std::string str(System::getHostName());

    ASSERT_EQ(str, System::getHostName());
}

TEST_F(SystemTest, test_pid)
{
    uint64_t pid = System::getPID();

    ASSERT_EQ(pid, System::getPID());
}

TEST_F(SystemTest, with_redirected_c_fstream)
{
    const std::string pattern("fun with file descriptors and streams");

    const auto res(youtils::System::with_captured_c_fstream(stdout,
                                                            [&]()
                                                            {
                                                                printf("%s",
                                                                       pattern.c_str());
                                                            }));

    EXPECT_EQ(pattern, res);
}

}

// Local Variables: **
// mode: c++ **
// End: **
