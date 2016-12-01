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

#ifndef YT_STRING_UTILS_H_
#define YT_STRING_UTILS_H_

#include <string>

namespace youtils
{

/* Return string describing error number */
std::string
safe_error_str(int error)
{
    char buf[1024];
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
    strerror_r(error, buf, 1024);
    std::string str(buf);
#else
    std::string str(strerror_r(error, buf, 1024));
#endif
    return str;
}

}

#endif // !YT_STRING_UTILS_H_
