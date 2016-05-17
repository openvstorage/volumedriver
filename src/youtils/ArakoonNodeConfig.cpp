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

#include "ArakoonNodeConfig.h"
#include "StreamUtils.h"

namespace arakoon
{

std::ostream&
operator<<(std::ostream& os,
           const ArakoonNodeConfig& cfg)
{
    return os << "(" << cfg.node_id_ << ", " << cfg.hostname_ << ", " << cfg.port_ << ")";
}

std::ostream&
operator<<(std::ostream& os,
           const ArakoonNodeConfigs& cfgs)
{
    return youtils::StreamUtils::stream_out_sequence(os,
                                                     cfgs);
}

}

namespace initialized_params
{

using Accessor = PropertyTreeVectorAccessor<arakoon::ArakoonNodeConfig>;

const std::string
Accessor::node_id_key("node_id");

const std::string
Accessor::host_key("host");

const std::string
Accessor::port_key("port");

}
