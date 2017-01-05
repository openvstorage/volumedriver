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

#include "AlbaConfig.h"

#include <iostream>

namespace backend
{

std::ostream&
AlbaConfig::stream_out(std::ostream& os) const
{
#define V(x)                                    \
    ((x).value())

    return os <<
        "AlbaConfig{host=" << V(alba_connection_host) <<
        ",port=" << V(alba_connection_port) <<
        ",timeout=" << V(alba_connection_timeout) <<
        ",transport=" << V(alba_connection_transport) <<
        ",use_rora=" << V(alba_connection_use_rora) <<
        ",rora_manifest_cache_capacity=" << V(alba_connection_rora_manifest_cache_capacity) <<
        ",rora_use_nullio=" << V(alba_connection_rora_use_nullio) <<
        ",preset=" << V(alba_connection_preset) <<
        "}";

#undef V
}

}
