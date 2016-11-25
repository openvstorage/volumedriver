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

DECLARE_LOGGER("ClusterRegistryUtils");

void
check_node_map(const ClusterRegistry::NodeStatusMap& map)
{
    for (const auto& p : map)
    {
        if (p.second.config.node_distance_map)
        {
            for (const auto& q : *p.second.config.node_distance_map)
            {
                auto it = map.find(q.first);
                if (it == map.end())
                {
                    LOG_ERROR("Node " << q.first << " present in NodeDistanceMap of " <<
                              p.first << " but absent from NodeStatusMap");
                    throw InvalidConfigurationException("Node present in NodeDistanceMap but absent from NodeStatusMap",
                                                        q.first.str().c_str(),
                                                        EINVAL);
                }
            }
        }
    }
}

std::string
serialize_node_map(const ClusterRegistry::NodeStatusMap& map)
{
    check_node_map(map);

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
    NodeStatusMap old_map;
    boost::optional<ara::buffer> buf;

    try
    {
        buf = get_node_status_map_();
        old_map = deserialize_node_map(*buf);
    }
    catch (ClusterNotRegisteredException&)
    {}

    NodeStatusMap new_map;

    if (configs.empty())
    {
        LOG_ERROR("Empty cluster configuration not permitted");
        throw InvalidConfigurationException("empty cluster configuration not permitted",
                                            cluster_id_.str().c_str());
    }

    for (const auto& cfg : configs)
    {
        ClusterNodeStatus::State state = ClusterNodeStatus::State::Online;
        const auto it = old_map.find(cfg.vrouter_id);
        if (it != old_map.end())
        {
            state = it->second.state;
        }

        const auto res(new_map.emplace(cfg.vrouter_id,
                                       ClusterNodeStatus(cfg,
                                                         state)));
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
                               const std::string key(make_key());
                               if (buf)
                               {
                                   seq.add_assert(key,
                                                  *buf);
                               }
                               else
                               {
                                   seq.add_assert(key,
                                                  ara::None());
                               }

                               seq.add_set(key,
                                           serialize_node_map(new_map));
                           },
                           yt::RetryOnArakoonAssert::F);

    LOG_INFO("Registry for cluster " << cluster_id_ << " initialized");
}

ara::buffer
ClusterRegistry::get_node_status_map_()
try
{
    LOG_TRACE("getting the node status map for " << cluster_id_);
    return arakoon_->get(make_key());
}
catch (ara::error_not_found&)
{
    throw ClusterNotRegisteredException("cluster not registered",
                                        cluster_id_.str().c_str());
}

ClusterRegistry::NodeStatusMap
ClusterRegistry::get_node_status_map()
{
    return deserialize_node_map(get_node_status_map_());
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

ClusterNodeStatus
ClusterRegistry::get_node_status(const NodeId& node_id)
{
    LOG_TRACE(node_id);

    const NodeStatusMap map(get_node_status_map());
    const auto it = find_node_throw_(node_id, map);
    LOG_TRACE(node_id << ", state " << it->second.state);

    return it->second;
}

void
ClusterRegistry::set_node_state(const NodeId& node_id,
                                ClusterNodeStatus::State state)
try
{
    LOG_TRACE(node_id << ", state " << state);

    arakoon_->run_sequence("set node state",
                           [&](ara::sequence& seq)
                           {
                               const ara::buffer buf(get_node_status_map_());
                               NodeStatusMap map(deserialize_node_map(buf));

                               auto it = find_node_throw_(node_id, map);
                               const ClusterNodeStatus::State old_state = it->second.state;
                               it->second = ClusterNodeStatus(it->second.config, state);

                               LOG_INFO("Updating state of node " << node_id <<
                                        " from " << old_state << " to " << state);

                               const std::string key(make_key());
                               seq.add_assert(key,
                                              buf);

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
    const ara::buffer buf(get_node_status_map_());
    const NodeStatusMap map(deserialize_node_map(buf));
    const auto it = find_node_throw_(node_id, map);

    const ClusterNodeStatus::State state = it->second.state;
    if (state != ClusterNodeStatus::State::Offline)
    {
        LOG_ERROR(node_id << " is in the wrong state " << state << " - bailing out");
        throw ClusterNodeNotOfflineException("node not online",
                                             node_id.str().c_str());
    }

    seq.add_assert(make_key(),
                   buf);
}

ClusterRegistry::NeighbourMap
ClusterRegistry::get_neighbour_map(const NodeId& node_id)
{
    const NodeStatusMap status_map(get_node_status_map());
    auto it = find_node_throw_(node_id,
                               status_map);

    const ClusterNodeConfig& cfg = it->second.config;
    ClusterRegistry::NeighbourMap neigh_map;

    if (cfg.node_distance_map)
    {
        for (const auto& p : *cfg.node_distance_map)
        {
            auto it = status_map.find(p.first);
            if (it == status_map.end())
            {
                // throw instead?
                LOG_WARN(p.first << " present in NodeDistanceMap of " <<
                         node_id << " but absent from NodeStatusMap - skipping it");
            }
            else
            {
                neigh_map.emplace(p.second, it->second.config);
            }
        }
    }
    else
    {
        for (const auto& p : status_map)
        {
            neigh_map.emplace(0, p.second.config);
        }
    }

    return neigh_map;
}

}
