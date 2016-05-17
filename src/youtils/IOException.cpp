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

#include "IOException.h"
#include <cstring>
#include <sstream>

namespace fungi {

IOException::IOException(const char *msg, const char *name, int error_code)
    : error_code_(error_code),
      msg_(msg)
{
    std::stringstream ss;
    ss << " ";
    if (name != 0) {
        ss << name;
    }
    if (error_code != 0) {
        ss << " ";
        char *res = strerror(error_code);
        if (res != 0) {
            ss << res;
        }
        ss << " (" << error_code << ")";
    }
    msg_ += ss.str();
}

IOException::IOException(std::string&& thing)
{
    msg_ = std::move(thing);
}

IOException::IOException(const std::string& thing)
{
    msg_ = std::move(thing);
}

IOException::~IOException() noexcept
{
}

const char *IOException::what() const noexcept
{
    return msg_.c_str();
}

int IOException::getErrorCode() const
{
    return error_code_;
}

}
