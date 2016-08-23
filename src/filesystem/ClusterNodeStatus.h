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

#ifndef VFS_CLUSTER_NODE_STATUS_H_
#define VFS_CLUSTER_NODE_STATUS_H_

#include "ClusterNodeConfig.h"

#include <iosfwd>

#include <youtils/Serialization.h>

namespace volumedriverfs
{
// make it a std::pair<ClusterNodeConfig, ClusterNodeStatus> and get all this for free?
struct ClusterNodeStatus
{
    enum class State
    {
        Offline,
        Online
    };

    ClusterNodeStatus(const ClusterNodeConfig& cfg,
                      State st)
        : config(cfg)
        , state(st)
    {}

    ~ClusterNodeStatus() = default;

    ClusterNodeStatus(const ClusterNodeStatus& other)
        : config(other.config)
        , state(other.state)
    {}

    ClusterNodeStatus&
    operator=(const ClusterNodeStatus& other)
    {
        if (this != &other)
        {
            const_cast<ClusterNodeConfig&>(config) = other.config;
            const_cast<State&>(state) = other.state;
        }

        return *this;
    }

    bool
    operator==(const ClusterNodeStatus& other)
    {
        return
            (config == other.config) and
            (state == other.state);
    }

    bool
    operator!=(const ClusterNodeStatus& other)
    {
        return not (*this == other);
    }

    const ClusterNodeConfig config;
    const State state;

    template<class Archive>
    void
    serialize(Archive& ar, const unsigned int version)
    {
        CHECK_VERSION(version, 1);
        ar & const_cast<ClusterNodeConfig&>(config);
        ar & const_cast<State&>(state);
    }
};

std::ostream&
operator<<(std::ostream& os, ClusterNodeStatus::State st);

}

BOOST_CLASS_VERSION(volumedriverfs::ClusterNodeStatus, 1);

namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
load_construct_data(Archive&,
                    volumedriverfs::ClusterNodeStatus* status,
                    const unsigned int /* version */)
{
    using namespace volumedriverfs;

    ClusterNodeConfig cfg(NodeId("uninitialized"),
                          "",
                          MessagePort(0),
                          XmlRpcPort(0),
                          FailoverCachePort(0),
                          youtils::Uri());

    new(status) ClusterNodeStatus(cfg,
                                  ClusterNodeStatus::State::Offline);
}

}

}

#endif // !VFS_CLUSTER_NODE_STATUS_H_
