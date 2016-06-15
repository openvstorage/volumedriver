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
