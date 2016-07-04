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

#include "../ThrowingDestructor.h"

#include <gtest/gtest.h>

#include <memory>

namespace youtilstest
{
using namespace youtils;

class UnWellBehaved : public ThrowingDestructor
{
public:
    ~UnWellBehaved()
    {
        if(throw_in_destructor())
        {
            throw 1;
        }
        else
        {

            // eat it, LOG an ERROR do something else but DONT THROW!!
        }

    }

};

class ThrowingDestructorTest : public testing::Test
{};

TEST_F(ThrowingDestructorTest, DISABLED_test)
{
    //  create the class
    {
        auto WillThrowInDestructor = std::make_unique<UnWellBehaved>();
        // No problem with the destructor
    }

    auto WillThrowInDestructor = std::make_unique<UnWellBehaved>();
    WillThrowInDestructor->throw_in_destructor(EnableThrowInDestructor::T);

}

}
