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

#include <gtest/gtest.h>
#include "../ScopeExit.h"
#include "../IOException.h"

namespace youtilstest
{
using namespace youtils;

class ScopedExitTest : public testing::Test
{
public:

    bool test_bool;

    MAKE_EXCEPTION(ScopedExitTestException, fungi::IOException);

};

TEST_F(ScopedExitTest, test1)
{
    test_bool = false;
    ASSERT_FALSE(test_bool);

    EXPECT_THROW({
            auto on_exit_1 = make_scope_exit([this](void)
                                             {
                                                 test_bool = true;
                                             });
            ASSERT_FALSE(test_bool);

            throw ScopedExitTestException("blah");
        },
        ScopedExitTestException);

    ASSERT_TRUE(test_bool);
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

TEST_F(ScopedExitTest, exceptional)
{
    bool exception = false;

    {
        auto on_exit(make_scope_exit_on_exception([&]
                                                  {
                                                      exception = true;
                                                  }));
    }

    EXPECT_FALSE(exception);

    EXPECT_THROW({
            auto on_exit(make_scope_exit_on_exception([&]
                                                      {
                                                          exception = true;
                                                      }));
            throw ScopedExitTestException("catch me");
        },
        ScopedExitTestException);

    EXPECT_TRUE(exception);
}

}

// Local Variables: **
// mode: c++ **
// End: **
