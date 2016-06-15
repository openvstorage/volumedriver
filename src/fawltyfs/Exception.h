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

#ifndef FAWLTY_FS_EXCEPTION_H_
#define FAWLTY_FS_EXCEPTION_H_

#include <exception>
#include <string>

namespace fawltyfs
{

class Exception
    : public std::exception
{
public:
    explicit Exception(const std::string& msg)
        : msg_(msg)
    {}

    virtual ~Exception() throw ()
    {}

    virtual const char*
    what() const throw ()
    {
        return msg_.c_str();
    }

private:
    const std::string msg_;
};

#define DECLARE_EXCEPTION(ex, parent)           \
    class ex                                    \
        : public parent                         \
    {                                           \
        public:                                 \
        explicit ex(const std::string& msg)     \
            : parent(msg)                       \
        {}                                      \
    };
}

#endif // !FAWLTY_FS_EXCEPTION_H_

// Local Variables: **
// compile-command: "scons -D -j 4" **
// mode: c++ **
// End: **
