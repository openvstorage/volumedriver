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
#include "../Chooser.h"

namespace youtilstest
{
using namespace youtils;

class ChooserTest : public TestBase
{};

/*
enum class Choices
{
    Vanilla,
    Strawberry,
    Cherry
        };

TEST_F(ChooserTest, test_construction_with_odds)
{
    //using namespace youtils;



    Chooser<Choices> chooser(std::vector<std::pair<Choices,double> >({
                {Choices::Vanilla, 0.3},
                {Choices::Strawberry, 0.3},
                {Choices::Cherry, 0.4}
            }));

    std::map<Choices, int> test_map;
    const unsigned test_size = 1024*1024*128;

    for(unsigned i = 0; i < test_size; ++i)
    {
        ++test_map[chooser()];
    }
    ASSERT_NEAR((double)test_map[Choices::Vanilla]/(double)test_size, 0.3, 0.0001);
    ASSERT_NEAR((double)test_map[Choices::Strawberry]/(double)test_size, 0.3, 0.0001);
    ASSERT_NEAR((double)test_map[Choices::Cherry]/(double)test_size, 0.4, 0.0001);
}

TEST_F(ChooserTest, test_construction_with_weights)
{
    //using namespace youtils;

    Chooser<Choices> chooser(std::vector<std::pair<Choices,unsigned> >({
        {Choices::Vanilla, 2},
        {Choices::Strawberry, 4},
        {Choices::Cherry, 6}
        }));

    std::map<Choices, int> test_map;
    const unsigned test_size = 1024*1024*128;

    for(unsigned i = 0; i < test_size; ++i)
    {
        ++test_map[chooser()];
    }
    ASSERT_NEAR((double)test_map[Choices::Vanilla]/(double)test_size, 2.0/12.0, 0.0001);
    ASSERT_NEAR((double)test_map[Choices::Strawberry]/(double)test_size, 4.0/12.0, 0.0001);
    ASSERT_NEAR((double)test_map[Choices::Cherry]/(double)test_size, 0.5, 0.0001);
}
*/
}


// Local Variables: **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// mode: c++ **
// End: **
