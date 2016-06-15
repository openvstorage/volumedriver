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

#ifndef BACKEND_NAMES_FILTER_H_
#define BACKEND_NAMES_FILTER_H_

#include <string>

#include <boost/regex.hpp>

#include <youtils/Logging.h>

namespace volumedriver
{
class BackendNamesFilter
{
public:
    bool
    operator()(const std::string& name) const;

private:
    DECLARE_LOGGER("BackendNamesFilter");

    static const boost::regex&
    regex_();
};
}

#endif // !BACKEND_NAMES_FILTER_H_

// Local Variables: **
// mode: c++ **
// compile-command: "scons -D --kernel_version=system --ignore-buildinfo -j 5" **
// End: **
