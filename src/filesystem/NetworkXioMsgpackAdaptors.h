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

#ifndef NETWORK_XIO_MSGPACK_ADAPTORS_H_
#define NETWORK_XIO_MSGPACK_ADAPTORS_H_

#include <msgpack.hpp>
#include <backend/Namespace.h>
#include <volumedriver/Types.h>

namespace msgpack
{
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
{
namespace adaptor
{

template<>
struct pack<backend::Namespace>
{
    template<typename Stream>
    packer<Stream>& operator()(msgpack::packer<Stream>& p,
                               backend::Namespace const& ns) const
    {
        p.pack(ns.str());
        return p;
    }
};

template<>
struct pack<volumedriver::SCOCloneID>
{
    template<typename Stream>
    packer<Stream>& operator()(msgpack::packer<Stream>& p,
                               volumedriver::SCOCloneID const& id) const
    {
        p.pack(static_cast<uint8_t>(id));
        return p;
    }
};

template<>
struct pack<volumedriver::ClusterLocationAndHash>
{
    template<typename Stream>
    packer<Stream>& operator()(msgpack::packer<Stream>& p,
                               volumedriver::ClusterLocationAndHash const& cl) const
    {
        p.pack_uint64(*reinterpret_cast<const uint64_t*>(&cl.clusterLocation));
        return p;
    }
};

} //namespace adaptor
} //MSGPACK_DEFAULT_API_NS
} //namespace msgpack

#endif // NETWORK_XIO_MSGPACK_ADAPTORS_H_
