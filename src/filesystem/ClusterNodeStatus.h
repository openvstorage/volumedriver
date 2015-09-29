// Copyright 2015 Open vStorage NV
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
                          FailoverCachePort(0));

    new(status) ClusterNodeStatus(cfg,
                                  ClusterNodeStatus::State::Offline);
}

}

}

#endif // !VFS_CLUSTER_NODE_STATUS_H_
