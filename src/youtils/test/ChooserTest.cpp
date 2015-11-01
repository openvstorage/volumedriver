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
