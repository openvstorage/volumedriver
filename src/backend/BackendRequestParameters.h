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

#ifndef BACKEND_REQUEST_PARAMETERS_H_
#define BACKEND_REQUEST_PARAMETERS_H_

#include <boost/optional.hpp>

namespace backend
{

struct BackendRequestParameters
{
#define MAKE_PARAM(NAME, TYPE)                  \
    BackendRequestParameters&                   \
    NAME(const boost::optional<TYPE>& val)      \
    {                                           \
        NAME ## _ = val;                        \
        return *this;                           \
    }                                           \
                                                \
    boost::optional<TYPE> NAME ## _

    MAKE_PARAM(retries_on_error, uint32_t) = boost::none;
    MAKE_PARAM(retry_interval, boost::chrono::milliseconds) = boost::none;
    MAKE_PARAM(retry_backoff_multiplier, double) = boost::none;
    MAKE_PARAM(timeout, boost::posix_time::time_duration) = boost::none;

#undef MAKE_PARAM

    // clang wants this one:
    // backend/BackendInterface.cpp:42:43: error: default initialization of an object of const type 'const backend::BackendRequestParameters' without a user-provided default constructor
    // static const BackendRequestParameters params;
    BackendRequestParameters()
    {}

    ~BackendRequestParameters() = default;
};

}

#endif // !BACKEND_REQUEST_PARAMETERS_H_
