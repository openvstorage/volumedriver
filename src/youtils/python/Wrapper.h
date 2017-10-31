// Copyright (C) 2017 iNuron NV
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

#ifndef YPY_WRAPPER_H_
#define YPY_WRAPPER_H_

namespace youtils
{

namespace python
{

template<typename T>
void
register_once()
{
    static bool registered = false;
    if (not registered)
    {
        T::registerize();
        registered = true;
    }
}

}

}

#define DECLARE_PYTHON_WRAPPER_METHOD()         \
    static void                                 \
    registerize()

#define DECLARE_PYTHON_WRAPPER(T)               \
    struct T                                    \
    {                                           \
    private:                                    \
        template<typename>                      \
        friend void                             \
        youtils::python::register_once();       \
                                                \
        DECLARE_PYTHON_WRAPPER_METHOD();        \
    }

#define DEFINE_PYTHON_WRAPPER(T)         \
    void                                 \
    T::registerize()

#endif // !YPY_WRAPPER_H_
