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

// TODO:
// Completely converting this one to use URIs would make it nicer / more consistent but
// we're stuck with the current mix for backward compat reasons. Try cleaning this up over
// a few releases?
struct ClusterNodeConfig
{
    using MaybeUri = boost::optional<youtils::Uri>;
    using NodeDistanceMap = std::map<NodeId, uint32_t>;
    using MaybeNodeDistanceMap = boost::optional<NodeDistanceMap>;

    ClusterNodeConfig(const NodeId& id,
                      const std::string& mhost,
                      MessagePort mport,
                      XmlRpcPort xport,
                      FailoverCachePort fport,
                      const MaybeUri& nw_uri,
                      const boost::optional<std::string>& xhost = boost::none,
                      const boost::optional<std::string>& fhost = boost::none,
                      const MaybeNodeDistanceMap& node_dist_map = boost::none)
        : vrouter_id(id)
        , message_host(mhost)
        , message_port(mport)
        , xmlrpc_host(xhost ? *xhost : mhost)
        , xmlrpc_port(xport)
        , failovercache_host(fhost ? *fhost : mhost)
        , failovercache_port(fport)
        , network_server_uri(nw_uri)
        , node_distance_map(node_dist_map)
    {
        //we don't allow empty node_id's as the empty string is
        //used in our python API as "don't care"
        THROW_WHEN(vrouter_id.empty());
    }

    ~ClusterNodeConfig() = default;

    ClusterNodeConfig(const ClusterNodeConfig& other)
        : vrouter_id(other.vrouter_id)
        , message_host(other.message_host)
        , message_port(other.message_port)
        , xmlrpc_host(other.xmlrpc_host)
        , xmlrpc_port(other.xmlrpc_port)
        , failovercache_host(other.failovercache_host)
        , failovercache_port(other.failovercache_port)
        , network_server_uri(other.network_server_uri)
        , node_distance_map(other.node_distance_map)
    {}

    ClusterNodeConfig&
    operator=(const ClusterNodeConfig& other)
    {
        if (this != &other)
        {
            vrouter_id = other.vrouter_id;
            message_host = other.message_host;
            message_port = other.message_port;
            xmlrpc_host = other.xmlrpc_host;
            xmlrpc_port = other.xmlrpc_port;
            failovercache_host = other.failovercache_host;
            failovercache_port = other.failovercache_port;
            network_server_uri = other.network_server_uri;
            node_distance_map = other.node_distance_map;
        }

        return *this;
    }

    bool
    operator==(const ClusterNodeConfig& other) const
    {
        return
            (vrouter_id == other.vrouter_id) and
            (message_host == other.message_host) and
            (message_port == other.message_port) and
            (xmlrpc_host == other.xmlrpc_host) and
            (xmlrpc_port == other.xmlrpc_port) and
            (failovercache_host == other.failovercache_host) and
            (failovercache_port == other.failovercache_port) and
            (network_server_uri == other.network_server_uri) and
            (node_distance_map == other.node_distance_map);
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
        if (version > 4)
        {
            THROW_SERIALIZATION_ERROR(version, 4, 1);
        }

        ar & vrouter_id;
        ar & message_host;
        ar & static_cast<uint16_t&>(message_port);
        ar & static_cast<uint16_t&>(xmlrpc_port);
        ar & static_cast<uint16_t&>(failovercache_port);

        if (version > 1)
        {
            ar & network_server_uri;
        }
        else
        {
            network_server_uri = boost::none;
        }

        if (version > 2)
        {
            ar & xmlrpc_host;
            ar & failovercache_host;
        }
        else
        {
            xmlrpc_host = message_host;
            failovercache_host = message_host;
        }

        if (version > 3)
        {
            ar & node_distance_map;
        }
        else
        {
            node_distance_map = boost::none;
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned version) const
    {
        CHECK_VERSION(version, 4);

        ar & vrouter_id;
        ar & message_host;
        ar & static_cast<const uint16_t&>(message_port);
        ar & static_cast<const uint16_t&>(xmlrpc_port);
        ar & static_cast<const uint16_t&>(failovercache_port);
        ar & network_server_uri;
        ar & xmlrpc_host;
        ar & failovercache_host;
        ar & node_distance_map;
    }

    // for python consumption
    std::string
    str() const;

    NodeId vrouter_id;
    std::string message_host;
    MessagePort message_port;
    std::string xmlrpc_host;
    XmlRpcPort xmlrpc_port;
    std::string failovercache_host;
    FailoverCachePort failovercache_port;
    MaybeUri network_server_uri;
    MaybeNodeDistanceMap node_distance_map;
};

std::ostream&
operator<<(std::ostream&,
           const ClusterNodeConfig&);

using ClusterNodeConfigs = std::vector<ClusterNodeConfig>;

}

BOOST_CLASS_VERSION(volumedriverfs::ClusterNodeConfig, 4);

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
