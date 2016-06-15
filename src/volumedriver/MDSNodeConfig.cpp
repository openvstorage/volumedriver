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

#include "MDSNodeConfig.h"

#include <iostream>

#include <youtils/Assert.h>
#include <youtils/StreamUtils.h>

TODO("AR: push this thing to the metadata-server location and namespace?");

namespace volumedriver
{

std::ostream&
operator<<(std::ostream& os,
           const MDSNodeConfig& cfg)
{
    os << "mds://" << cfg.address() << ":" << cfg.port();
    return os;
}

std::ostream&
operator<<(std::ostream& os,
           const MDSNodeConfigs& cfgs)
{
    return youtils::StreamUtils::stream_out_sequence(os,
                                                     cfgs);
}

}

namespace initialized_params
{

using Accessor = PropertyTreeVectorAccessor<volumedriver::MDSNodeConfig>;

const std::string
Accessor::host_key("host");

const std::string
Accessor::port_key("port");

}
