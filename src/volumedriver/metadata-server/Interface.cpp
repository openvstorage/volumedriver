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

#include "Interface.h"

#include <iostream>

namespace metadata_server
{

std::ostream&
operator<<(std::ostream& os,
           const TableCounters& c)
{
    return os <<
        "TableCounters{total_tlogs_read=" << c.total_tlogs_read <<
        ",incremental_updates=" << c.incremental_updates <<
        ",full_rebuilds=" << c.full_rebuilds <<
        "}";
}

}
