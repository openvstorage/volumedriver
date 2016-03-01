// Copyright 2016 iNuron NV
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
