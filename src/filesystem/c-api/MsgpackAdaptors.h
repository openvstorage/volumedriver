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

#ifndef __MSGPACK_ADAPTORS_H
#define __MSGPACK_ADAPTORS_H

#include "common_priv.h"

#include <youtils/Assert.h>

PRAGMA_IGNORE_WARNING_BEGIN("-Wctor-dtor-privacy")
#include <msgpack.hpp>
PRAGMA_IGNORE_WARNING_END;

namespace msgpack
{
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
{
namespace adaptor
{

template<>
struct convert<libovsvolumedriver::ClusterLocation> {
    msgpack::object const& operator()(msgpack::object const& o,
                                      libovsvolumedriver::ClusterLocation& v) const
    {
        *reinterpret_cast<uint64_t*>(&v) = o.via.u64;
        return o;
    }
};

} //namespace adaptor
} //MSGPACK_DEFAULT_API_NS
} //namespace msgpack


#endif // __MSGPACK_ADAPTORS_H
