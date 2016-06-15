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

#include "TLogId.h"

#include <iostream>

namespace volumedriver
{

namespace yt = youtils;

namespace
{

const std::string tlog_prefix("tlog_");

}

std::ostream&
operator<<(std::ostream& os,
           const TLogId& tlog_id)
{
    return os << tlog_prefix << tlog_id.t;
}

std::istream&
operator>>(std::istream& is,
           TLogId& tlog_id)
{
    std::string s;
    is >> s;
    if (s.substr(0, tlog_prefix.size()) == tlog_prefix)
    {
        try
        {
            yt::UUID uuid(s.substr(tlog_prefix.size()));
            tlog_id = std::move(uuid);
        }
        catch (...)
        {
            is.setstate(std::ios_base::failbit);
        }
    }
    else
    {
        is.setstate(std::ios_base::failbit);
    }

    return is;
}

}
