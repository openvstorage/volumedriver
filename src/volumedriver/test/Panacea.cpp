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

#include "ExGTest.h"
#include <iostream>
#include <unistd.h>
#include <vector>
#include <string>
#include <algorithm>

namespace volumedrivertest
{
using namespace volumedriver;

class Panacea : public ExGTest
{};

TEST_F(Panacea, DISABLED_test_aleph0)
{
    const std::vector<std::string> hostnames = { "voodoochile",
                                                 "blackadder"
    };

    char hn[1024];

    int res = gethostname(hn, 1024);
    ASSERT_TRUE(res == 0);

    if(std::find(hostnames.begin(), hostnames.end(), std::string(hn)) == hostnames.end())
    {
        // exit status 139 on my machine
        // special construct to catch crashes on jenkins and that once were not reported correctly
        std::cout << *static_cast<int*>(0) << std::endl; // "Remove this test if Hudson fails on it"
    }
}
}


// Local Variables: **
// mode: c++ **
// End: **
