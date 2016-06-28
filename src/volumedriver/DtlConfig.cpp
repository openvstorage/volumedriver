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

#include "DtlConfig.h"

#include <iostream>

#include <boost/lexical_cast.hpp>

namespace volumedriver
{

std::ostream&
operator<<(std::ostream& os,
           const DtlConfig& cfg)
{
    return os << "foc://" << cfg.host << ":" << cfg.port << "," << cfg.mode;
}

std::istream&
operator>>(std::istream& is,
           DtlConfig& cfg)
{
    char prefix[7];
    is.get(prefix, sizeof(prefix));
    if (std::string("foc://") == (const char *) prefix)
    {
        std::getline(is, cfg.host, ':');
        std::string port;
        std::getline(is, port, ',');
        cfg.port = boost::lexical_cast<uint16_t>(port);
        std::string mode;
        std::getline(is, mode);
        cfg.mode = boost::lexical_cast<DtlMode>(port);
    }
    return is;
}

}
