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

#include "ClusterRegistry.h"
#include "ObjectRouter.h" // ClusterNodeNotOfflineException

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/map.hpp>

#include <youtils/ArakoonInterface.h>
#include <youtils/Assert.h>

namespace volumedriverfs
{

namespace ara = arakoon;
namespace yt = youtils;

typedef boost::archive::text_iarchive iarchive_type;
typedef boost::archive::text_oarchive oarchive_type;

ClusterRegistry::ClusterRegistry(const ClusterId& cluster_id,
                                 std::shared_ptr<yt::LockedArakoon> arakoon)
    : cluster_id_(cluster_id)
    , arakoon_(arakoon)
{
    VERIFY(arakoon_);
}

std::string
ClusterRegistry::make_key() const
{
    static const std::string sfx("/cluster_configuration");
    return cluster_id_.str() + sfx;
}

namespace
{

std::string
serialize_node_map(const ClusterRegistry::NodeStatusMap& map)
{
    std::stringstream ss;
    oarchive_type oa(ss);
    oa << map;
    return ss.str();
}

ClusterRegistry::NodeStatusMap
deserialize_node_map(const ara::buffer& buf)
{
    return buf.as_istream<ClusterRegistry::NodeStatusMap>([](std::istream& is) ->
                                                          ClusterRegistry::NodeStatusMap
        {
            iarchive_type ia(is);
            ClusterRegistry::NodeStatusMap map;
            ia >> map;
            return map;
        });
}

}

ClusterRegistry::NodeStatusMap::iterator
ClusterRegistry::find_node_throw_(const NodeId& node_id,
                                  ClusterRegistry::NodeStatusMap& map)
{
    const auto it = map.find(node_id);
    if (it == map.end())
    {
        LOG_ERROR("Node " << node_id << " not registered in cluster " << cluster_id_);
        throw ClusterNodeNotRegisteredException("node not registered",
                                                node_id.str().c_str());
    }

    return it;
}

ClusterRegistry::NodeStatusMap::const_iterator
ClusterRegistry::find_node_throw_(const NodeId& node_id,
                                  const ClusterRegistry::NodeStatusMap& map)
{
    auto it = map.find(node_id);
    if (it == map.end())
    {
        LOG_ERROR("Node " << node_id << " not registered in cluster " << cluster_id_);
        throw ClusterNodeNotRegisteredException("node not registered",
                                                node_id.str().c_str());
    }

    return it;
}

void
ClusterRegistry::erase_node_configs()
{
    LOG_INFO(cluster_id_ << ": erasing node configs");

    try
    {
        arakoon_->run_sequence("erase node configs",
                               [this](ara::sequence& seq)
                               {
                                   seq.add_delete(make_key());
                               },
                               yt::RetryOnArakoonAssert::F);
    }
    catch (ara::error_not_found&)
    {
        LOG_INFO("Cannot remove node configs for " << cluster_id_ <<
                  " - cluster is not registered.");
    }
}

void
ClusterRegistry::set_node_configs(const ClusterNodeConfigs& configs)
{
    NodeStatusMap map;

    if (configs.empty())
    {
        LOG_ERROR("Empty cluster configuration not permitted");
        throw InvalidConfigurationException("empty cluster configuration not permitted",
                                            cluster_id_.str().c_str());
    }

    for (const auto& cfg : configs)
    {
        ClusterNodeStatus st(cfg, ClusterNodeStatus::State::Online);
        const auto res(map.insert(std::make_pair(cfg.vrouter_id, st)));
        if (not res.second)
        {
            LOG_ERROR("Duplicate node ID " << cfg.vrouter_id << " in cluster config");
            throw InvalidConfigurationException("duplicate node ID in cluster config",
                                                cfg.vrouter_id.str().c_str());
        }
    }

    arakoon_->run_sequence("set node configs",
                           [&](ara::sequence& seq)
                           {
                               seq.add_set(make_key(),
                                           serialize_node_map(map));
                           },
                           yt::RetryOnArakoonAssert::F);

    LOG_INFO("Registry for cluster " << cluster_id_ << " initialized");
}

ClusterRegistry::NodeStatusMap
ClusterRegistry::get_node_status_map()
try
{
    LOG_TRACE("getting the node status map for " << cluster_id_);

    const ara::buffer buf(arakoon_->get(make_key()));
    return deserialize_node_map(buf);
}
catch (ara::error_not_found&)
{
    throw ClusterNotRegisteredException("cluster not registered",
                                        cluster_id_.str().c_str());
}

ClusterNodeConfigs
ClusterRegistry::get_node_configs()
{
    LOG_TRACE("getting the node configs for " << cluster_id_);
    const NodeStatusMap map(get_node_status_map());

    ClusterNodeConfigs configs;
    configs.reserve(map.size());

    for (const auto& v : map)
    {
        configs.emplace_back(ClusterNodeConfig(v.second.config));
    }

    return configs;
}

ClusterNodeStatus::State
ClusterRegistry::get_node_state(const NodeId& node_id)
{
    LOG_TRACE(node_id);

    const NodeStatusMap map(get_node_status_map());
    const auto it = find_node_throw_(node_id, map);
    LOG_TRACE(node_id << ", state " << it->second.state);

    return it->second.state;
}

void
ClusterRegistry::set_node_state(const NodeId& node_id,
                                ClusterNodeStatus::State state)
try
{
    LOG_TRACE(node_id << ", state " << state);

    // The code could be refactored so we can cling onto the originally returned
    // ara::buffer instead of serializing the map here.
    // OTOH this should not be used in a critical path anyway, so let's not
    // bother for now.

    NodeStatusMap map(get_node_status_map());
    auto it = find_node_throw_(node_id, map);

    arakoon_->run_sequence("set node state",
                           [&](ara::sequence& seq)
                           {
                               const std::string key(make_key());
                               seq.add_assert(key, serialize_node_map(map));

                               LOG_INFO("Updating state of node " << node_id <<
                                        " from " << it->second.state << " to " << state);

                               it->second = ClusterNodeStatus(it->second.config, state);
                               seq.add_set(key, serialize_node_map(map));
                           },
                           yt::RetryOnArakoonAssert::T);
}
catch (ara::error_assertion_failed&)
{
    LOG_ERROR("Conflicting update detected while trying to update node " <<
              node_id);
    throw ConflictingClusterRegistryUpdateException("conflicting update");
}

void
ClusterRegistry::prepare_node_offline_assertion(arakoon::sequence& seq,
                                                const NodeId& node_id)
{
    const NodeStatusMap map(get_node_status_map());
    const auto it = find_node_throw_(node_id, map);

    const ClusterNodeStatus::State state = it->second.state;
    if (state != ClusterNodeStatus::State::Offline)
    {
        LOG_ERROR(node_id << " is in the wrong state " << state << " - bailing out");
        throw ClusterNodeNotOfflineException("node not online",
                                             node_id.str().c_str());
    }

    seq.add_assert(make_key(),
                   serialize_node_map(map));
}

}
