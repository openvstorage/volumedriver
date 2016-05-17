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
