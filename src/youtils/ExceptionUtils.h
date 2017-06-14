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

#ifndef YT_EXCEPTION_H_
#define YT_EXCEPTION_H_

#include <boost/exception_ptr.hpp>

namespace youtils
{

// there's no boost::make_exception_ptr (up to (and including) 1.62)
template<typename Ex>
static boost::exception_ptr
make_boost_exception_ptr(const Ex& e) noexcept
{
    try
    {
        throw e;
    }
    catch (...)
    {
        return boost::current_exception();
    }
};
}

#endif // !YT_EXCEPTION_H_
