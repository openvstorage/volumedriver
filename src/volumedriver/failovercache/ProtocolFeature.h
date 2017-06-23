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

#ifndef VD_FAILOVERCACHE_PROTOCOL_FEATURE_H_
#define VD_FAILOVERCACHE_PROTOCOL_FEATURE_H_

#include <cstdint>
#include <iosfwd>
#include <boost/serialization/strong_typedef.hpp>

namespace volumedriver
{

namespace failovercache
{

enum ProtocolFeature
{
    TunnelCapnProto = 0x1,
};

BOOST_STRONG_TYPEDEF(uint64_t, ProtocolFeatures);

std::ostream&
operator<<(std::ostream&,
           ProtocolFeature);

}

}

#endif // !VD_FAILOVERCACHE_PROTOCOL_FEATURE_H_
