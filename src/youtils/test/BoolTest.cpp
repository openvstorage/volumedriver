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
#include <boost/static_assert.hpp>

namespace youtilstest
{
//using namespace youtils;

class BoolTest : public testing::Test
{
public:

    void
    testMe(bool ,
           int )
    {
        //        std::cout << a << " and " << b;
    }

};

TEST_F(BoolTest, test1)
{
    bool B1(true);
    ASSERT_TRUE(B1);


    // Bool B2(1);

    //    testMe(B1, 1);
    //    B1 = false;
    //    ASSERT_FALSE(B1);



#ifdef TEST_COMPILATION
    int a = 1;
    testMe(a, B1);
    B1 = 1;

#endif // TEST_COMPILATION
    if(not B1)
    {
        ASSERT_FALSE(B1);
    }
}


}

// Local Variables: **
// bvirtual-targets: ("bin/vd_test") **
// mode: c++ **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// End: **
