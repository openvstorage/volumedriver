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

#include <functional>

template<typename... Args>
void
wrapper(void (*f)(Args...),
        Args... args)
{
    f(std::forward<Args...>(args...));
}

void
myfun(int)
{}

void
myfun2(const int)
{}

void
test()
{
    int a = 0;
    wrapper<int>(myfun, a);

    wrapper(myfun, a);

    wrapper<int>(myfun2, a);

    const int b = 0;
    wrapper(myfun2, b);
    //    http://gcc.gnu.org/bugzilla/show_bug.cgi?id=60608
    //    wrapper<const int>(myfun2, b);
}

/* Still need to get to the bottom of this one
class AClass
{};

class Handle
{};


AClass
makeAClass(const Handle&)
{
    return AClass();
}


template<typename... Args>
void
wrapper2(void (*f)(AClass&,
                   Args...),
         Handle h,
         Args... args)
{
    AClass a = makeAClass(h);

    f(a,
      std::forward<Args...>(args...));
}


void
myfun3(AClass&,
       double)
{}

void
myfun4(AClass&,
       const Handle&)
{}

void
test2()
{
    Handle h1;
    wrapper2(myfun3,
             h1,
             0.1);

    Handle h2;

    wrapper2(myfun4,
             h1,
             h2);

}

*/
