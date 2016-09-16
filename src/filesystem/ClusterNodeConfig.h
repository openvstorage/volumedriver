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

#ifndef VFS_CLUSTER_NODE_CONFIG_H_
#define VFS_CLUSTER_NODE_CONFIG_H_

#include "NodeId.h"

#include <iosfwd>
#include <sstream>
#include <vector>

#include <youtils/Assert.h>
#include <youtils/OurStrongTypedef.h>
#include <youtils/Serialization.h>
#include <youtils/Uri.h>

OUR_STRONG_ARITHMETIC_TYPEDEF(uint16_t, MessagePort, volumedriverfs);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint16_t, XmlRpcPort, volumedriverfs);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint16_t, FailoverCachePort, volumedriverfs);

namespace volumedriverfs
{

struct ClusterNodeConfig
{
    using MaybeUri = boost::optional<youtils::Uri>;

    ClusterNodeConfig(const NodeId& id,
                      const std::string& h,
                      MessagePort mport,
                      XmlRpcPort xport,
                      FailoverCachePort fport,
                      const MaybeUri& nw_uri)
        : vrouter_id(id)
        , host(h)
        , message_port(mport)
        , xmlrpc_port(xport)
        , failovercache_port(fport)
        , network_server_uri(nw_uri)
    {
        //we don't allow empty node_id's as the empty string is
        //used in our python API as "don't care"
        THROW_WHEN(vrouter_id.empty());
    }

    ~ClusterNodeConfig() = default;

    ClusterNodeConfig(const ClusterNodeConfig& other)
        : vrouter_id(other.vrouter_id)
        , host(other.host)
        , message_port(other.message_port)
        , xmlrpc_port(other.xmlrpc_port)
        , failovercache_port(other.failovercache_port)
        , network_server_uri(other.network_server_uri)
    {}

    ClusterNodeConfig&
    operator=(const ClusterNodeConfig& other)
    {
        if (this != &other)
        {
            const_cast<NodeId&>(vrouter_id) = other.vrouter_id;
            const_cast<std::string&>(host) = other.host;
            const_cast<MessagePort&>(message_port) = other.message_port;
            const_cast<XmlRpcPort&>(xmlrpc_port) = other.xmlrpc_port;
            const_cast<FailoverCachePort&>(failovercache_port) = other.failovercache_port;
            const_cast<MaybeUri&>(network_server_uri) = other.network_server_uri;
        }

        return *this;
    }

    bool
    operator==(const ClusterNodeConfig& other) const
    {
        return
            (vrouter_id == other.vrouter_id) and
            (host == other.host) and
            (message_port == other.message_port) and
            (xmlrpc_port == other.xmlrpc_port) and
            (failovercache_port == other.failovercache_port) and
            (network_server_uri == other.network_server_uri);
    }

    bool
    operator!=(const ClusterNodeConfig& other) const
    {
        return not (*this == other);
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned version)
    {
        if (version > 2)
        {
            THROW_SERIALIZATION_ERROR(version, 2, 1);
        }

        ar & const_cast<NodeId&>(vrouter_id);
        ar & const_cast<std::string&>(host);
        ar & static_cast<uint16_t&>(const_cast<MessagePort&>(message_port));
        ar & static_cast<uint16_t&>(const_cast<XmlRpcPort&>(xmlrpc_port));
        ar & static_cast<uint16_t&>(const_cast<FailoverCachePort&>(failovercache_port));

        if (version > 1)
        {
            ar & const_cast<MaybeUri&>(network_server_uri);
        }
        else
        {
            const_cast<MaybeUri&>(network_server_uri) = boost::none;
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned version) const
    {
        CHECK_VERSION(version, 2);

        ar & vrouter_id;
        ar & host;
        ar & static_cast<const uint16_t&>(message_port);
        ar & static_cast<const uint16_t&>(xmlrpc_port);
        ar & static_cast<const uint16_t&>(failovercache_port);
        ar & network_server_uri;
    }

    // for python consumption
    std::string
    str() const;

    const NodeId vrouter_id;
    const std::string host;
    const MessagePort message_port;
    const XmlRpcPort xmlrpc_port;
    const FailoverCachePort failovercache_port;
    const MaybeUri network_server_uri;
};

std::ostream&
operator<<(std::ostream&,
           const ClusterNodeConfig&);

using ClusterNodeConfigs = std::vector<ClusterNodeConfig>;

}

BOOST_CLASS_VERSION(volumedriverfs::ClusterNodeConfig, 2);

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive& ar,
                    volumedriverfs::ClusterNodeConfig* config,
                    const unsigned int /* version */)
{
    using namespace volumedriverfs;

    new(config) ClusterNodeConfig(NodeId("uninitialized"),
                                  "",
                                  MessagePort(0),
                                  XmlRpcPort(0),
                                  FailoverCachePort(0),
                                  youtils::Uri());
}

}

}

#endif // !VFS_CLUSTER_NODE_CONFIG_H_
