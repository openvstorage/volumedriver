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
