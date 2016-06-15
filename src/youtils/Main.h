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

#ifndef _MAIN_H_
#define _MAIN_H_

#include <boost/type_traits.hpp>

#include "MainHelper.h"
#include "TestMainHelper.h"

template <typename T>
int
the_main(int argc,
         char** argv)
{
    return T(argc, argv)();
}

#define MAIN(T)                                                         \
    int main(int argc, char** argv)                                     \
    {                                                                   \
        static_assert(boost::is_base_of<youtils::MainHelper, T>::value, \
                      "Was not a derived class of MainHelper");         \
        return the_main<T>(argc, argv);                                 \
    }

#endif

// Local Variables: **
// mode: c++ **
// End: **
