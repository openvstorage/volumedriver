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

#ifndef SCOPE_EXIT_H
#define SCOPE_EXIT_H

#include <utility>

namespace youtils
{

template<typename T, bool only_on_exception>
struct ScopeExit
{
    ScopeExit(T&& f) :
        f_(std::move(f))
    {}

    ~ScopeExit()
    {
        if (not only_on_exception or
            std::uncaught_exception())
        {
            f_();
        }
    }

    T f_;
};

template<typename T>
ScopeExit<T, false>
make_scope_exit(T&& f)
{
    return ScopeExit<T, false>(std::move(f));
}

template<typename T>
ScopeExit<T, true>
make_scope_exit_on_exception(T&& f)
{
    return ScopeExit<T, true>(std::move(f));
}

}

#endif // SCOPE_EXIT_H
