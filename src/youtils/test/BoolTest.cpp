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

#include "TestBase.h"
#include <boost/static_assert.hpp>

namespace youtilstest
{
//using namespace youtils;

class BoolTest : public TestBase
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
