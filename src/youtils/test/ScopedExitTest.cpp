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
#include "../ScopeExit.h"
#include "../IOException.h"

namespace youtilstest
{
using namespace youtils;

class ScopedExitTest : public TestBase
{
public:

    bool test_bool;

    MAKE_EXCEPTION(ScopedExitTestException, fungi::IOException);

};

TEST_F(ScopedExitTest, test1)
{

    test_bool = false;
    ASSERT_FALSE(test_bool);

    try
    {
        auto on_exit_1 = make_scope_exit([this](void)
                                         {
                                             test_bool = true;
                                         });
        ASSERT_FALSE(test_bool);

        throw ScopedExitTestException("blah");

    }
    catch(ScopedExitTestException& e)
    {
        ASSERT_TRUE(test_bool);
    }
}

TEST_F(ScopedExitTest, test2)
{
    test_bool = false;
    ASSERT_FALSE(test_bool);

    {
        auto on_exit_1 = make_scope_exit([this](void)
                                         {
                                             test_bool = true;
                                         });
        ASSERT_FALSE(test_bool);
    }
    ASSERT_TRUE(test_bool);
}
}

// Local Variables: **
// mode: c++ **
// End: **
