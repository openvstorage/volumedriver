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

#ifndef VFS_CLUSTER_REGISTRY_H_
#define VFS_CLUSTER_REGISTRY_H_

#include "ClusterId.h"
#include "ClusterNodeConfig.h"
#include "ClusterNodeStatus.h"
#include "LockedArakoon.h"
#include "NodeId.h"

#include <map>
#include <type_traits>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/Serialization.h>

namespace volumedriverfs
{

MAKE_EXCEPTION(ConflictingClusterRegistryUpdateException, fungi::IOException);
MAKE_EXCEPTION(ClusterNodeNotRegisteredException, fungi::IOException);
MAKE_EXCEPTION(ClusterNotRegisteredException, fungi::IOException);
MAKE_EXCEPTION(InvalidConfigurationException, fungi::IOException);

class ClusterRegistry
{
public:
    typedef std::map<NodeId, ClusterNodeStatus> NodeStatusMap;

    ClusterRegistry(const ClusterId& cluster_id,
                    std::shared_ptr<LockedArakoon> arakoon);

    template<typename T>
    ClusterRegistry(const ClusterId& cluster_id,
                    const arakoon::ClusterID& ara_cluster_id,
                    const T& arakoon_configs)
        : ClusterRegistry(cluster_id,
                          std::make_shared<LockedArakoon>(ara_cluster_id,
                                                          arakoon_configs,
                                                          arakoon::Cluster::MilliSeconds(2000)))
    {}

    ~ClusterRegistry() = default;

    ClusterRegistry(const ClusterRegistry&) = delete;

    ClusterRegistry&
    operator=(const ClusterRegistry&) = delete;

    void
    set_node_configs(const ClusterNodeConfigs& configs);

    ClusterNodeConfigs
    get_node_configs();

    void
    erase_node_configs();

    NodeStatusMap
    get_node_status_map();

    void
    set_node_state(const NodeId& id,
                   const ClusterNodeStatus::State st);

    ClusterNodeStatus::State
    get_node_state(const NodeId& id);

    ClusterId
    cluster_id() const
    {
        return cluster_id_;
    }

    void
    prepare_node_offline_assertion(arakoon::sequence& seq,
                                   const NodeId& node_id);

    std::string
    make_key() const;

private:
    DECLARE_LOGGER("ClusterRegistry");

    const ClusterId cluster_id_;
    std::shared_ptr<LockedArakoon> arakoon_;

    void
    verify_(const ClusterNodeConfigs& node_configs);

    NodeStatusMap::iterator
    find_node_throw_(const NodeId& node_id,
                     ClusterRegistry::NodeStatusMap& map);

    NodeStatusMap::const_iterator
    find_node_throw_(const NodeId& node_id,
                     const NodeStatusMap& map);
};

}

// Now, this is fun:
// In order to avoid default constructors for ClusterNodeStatus (and based on that,
// ClusterNodeConfig), we use {load,save}_construct_data specialisations.
// However, once these are put in a map, the code for std::pair (the map's value_type)
// wants to invoke the default constructors, so we need override that too.
namespace boost
{

namespace serialization
{

template<typename Archive>
inline void
serialize(Archive&,
          volumedriverfs::ClusterRegistry::NodeStatusMap::value_type&,
          const unsigned /* version */)
{
    // override that does nothing in favour of {load,save}_construct_data below
}

template<typename Archive>
inline void
save_construct_data(Archive& ar,
                    const volumedriverfs::ClusterRegistry::NodeStatusMap::value_type* pair,
                    const unsigned version)
{
    ar << make_nvp("first", pair->first);
    save_construct_data(ar, &pair->first, version);

    ar << make_nvp("second", pair->second);
    save_construct_data(ar, &pair->second, version);
}

template<typename Archive>
inline void
load_construct_data(Archive& ar,
                    volumedriverfs::ClusterRegistry::NodeStatusMap::value_type* pair,
                    const unsigned version)
{
    typedef volumedriverfs::ClusterRegistry::NodeStatusMap::value_type value_type;
    typedef std::remove_const<value_type::first_type>::type key_type;

    load_construct_data(ar, &const_cast<key_type&>(pair->first), version);
    ar >> make_nvp("first", const_cast<key_type&>(pair->first));

    load_construct_data(ar, &pair->second, version);
    ar >> make_nvp("second", pair->second);
}

}

}

#endif // !VFS_CLUSTER_REGISTRY_H_
