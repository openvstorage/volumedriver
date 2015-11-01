// Copyright 2015 iNuron NV
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef VFS_CLUSTER_NODE_CONFIG_H_
#define VFS_CLUSTER_NODE_CONFIG_H_

#include "NodeId.h"

#include <iosfwd>
#include <sstream>
#include <vector>

#include <youtils/Assert.h>
#include <youtils/OurStrongTypedef.h>
#include <youtils/Serialization.h>

OUR_STRONG_ARITHMETIC_TYPEDEF(uint16_t, MessagePort, volumedriverfs);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint16_t, XmlRpcPort, volumedriverfs);
OUR_STRONG_ARITHMETIC_TYPEDEF(uint16_t, FailoverCachePort, volumedriverfs);

namespace volumedriverfs
{

struct ClusterNodeConfig
{
    ClusterNodeConfig(const NodeId& id,
                      const std::string& h,
                      MessagePort mport,
                      XmlRpcPort xport,
                      FailoverCachePort fport)
        : vrouter_id(id)
        , host(h)
        , message_port(mport)
        , xmlrpc_port(xport)
        , failovercache_port(fport)
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
            (failovercache_port == other.failovercache_port);
    }

    bool
    operator!=(const ClusterNodeConfig& other) const
    {
        return not (*this == other);
    }

    template<class Archive>
    void
    serialize(Archive& ar, const unsigned version)
    {
        CHECK_VERSION(version, 1);

        ar & const_cast<NodeId&>(vrouter_id);
        ar & const_cast<std::string&>(host);
        ar & static_cast<uint16_t&>(const_cast<MessagePort&>(message_port));
        ar & static_cast<uint16_t&>(const_cast<XmlRpcPort&>(xmlrpc_port));
        ar & static_cast<uint16_t&>(const_cast<FailoverCachePort&>(failovercache_port));
    }

    // for python consumption
    std::string
    str() const;

    const NodeId vrouter_id;
    const std::string host;
    const MessagePort message_port;
    const XmlRpcPort xmlrpc_port;
    const FailoverCachePort failovercache_port;
};

std::ostream&
operator<<(std::ostream&,
           const ClusterNodeConfig&);

using ClusterNodeConfigs = std::vector<ClusterNodeConfig>;

}

BOOST_CLASS_VERSION(volumedriverfs::ClusterNodeConfig, 1);

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
                                  FailoverCachePort(0));
}

}

}

#endif // !VFS_CLUSTER_NODE_CONFIG_H_
