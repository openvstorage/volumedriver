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

#include "FailOverCacheCommand.h"

#include <iostream>

#include <boost/lexical_cast.hpp>

#include <youtils/Assert.h>

namespace volumedriver
{

std::ostream&
operator<<(std::ostream& os,
           FailOverCacheCommand cmd)
{
    switch (cmd)
    {
#define CASE(x)                                 \
        case FailOverCacheCommand::x:           \
            os << #x;                           \
            break

        CASE(RemoveUpTo);
        CASE(GetSCO);
        CASE(AddEntries);
        CASE(GetEntries);
        CASE(Flush);
        CASE(Ok);
        CASE(NotOk);
        CASE(Unregister);
        CASE(Clear);
        CASE(GetSCORange);

#undef CASE

    default:
        ASSERT("no translation for cmd" == boost::lexical_cast<std::string>(static_cast<uint32_t>(cmd)));
        os.setstate(std::ios_base::failbit);
        break;
    }

    return os;
}

}
