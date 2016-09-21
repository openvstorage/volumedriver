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

#include "ClusterNodeConfig.h"

#include <boost/lexical_cast.hpp>

namespace volumedriverfs
{

std::ostream&
operator<<(std::ostream& os,
           const ClusterNodeConfig& cfg)
{
    os << "ClusterNodeConfig { " <<
        "vrouter_id: \"" << cfg.vrouter_id << "\", " <<
        "message_host: \"" << cfg.message_host << "\", " <<
        "message_port: " << cfg.message_port << ", " <<
        "xmlrpc_host: \"" << cfg.xmlrpc_host << "\", " <<
        "xmlrpc_port: " << cfg.xmlrpc_port << ", " <<
        "failovercache_host: \"" << cfg.failovercache_host << "\", " <<
        "failovercache_port: " << cfg.failovercache_port << " }";

    return os;
}

std::string
ClusterNodeConfig::str() const
{
    return boost::lexical_cast<std::string>(*this);
}

}

/* namespace filesystem */
