// Copyright 2015 Open vStorage NV
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

#include "../ThrowingDestructor.h"
#include "../TestBase.h"

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

class ThrowingDestructorTest : public TestBase
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
