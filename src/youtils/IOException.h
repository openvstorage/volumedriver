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

#ifndef _IOEXCEPTION_H
#define	_IOEXCEPTION_H

#include <exception>
#include <string>

namespace fungi {

class IOException: public std::exception
{
public:
    IOException(const char *msg,
                const char *name = 0,
                int error_code = 0);

    IOException(std::string&& str);

    IOException(const std::string&);

    virtual ~IOException() noexcept;

    virtual const char* what() const noexcept;

    int getErrorCode() const;

private:
    int error_code_;
    std::string msg_;
};
}

// this macro can be used to easily make child exceptions
#define MAKE_EXCEPTION(myex, parentex)                                  \
    class myex: public parentex                                         \
    {                                                                   \
        public:                                                         \
        myex(const char *msg,                                           \
             const char *name = 0,                                      \
             int error_code = 0)                                        \
            : parentex(msg, name, error_code)                           \
        {}                                                              \
                                                                        \
        myex(std::string&& str)                                         \
            : parentex(std::move(str))                                  \
        {}                                                              \
                                                                        \
        myex(const std::string& str)                                    \
            : parentex(str)                                             \
        {}                                                              \
    };                                                                  \


namespace fungi {

MAKE_EXCEPTION(ResolvingError, IOException);
MAKE_EXCEPTION(ConnectionRefusedError, IOException);

}

#endif	/* _IOEXCEPTION_H */
