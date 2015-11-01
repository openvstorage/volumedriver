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
