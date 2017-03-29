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

#include "AmqpEventPublisher.h"
#include "EventPublisher.h"
#include "FileSystem.h"
#include "FileSystemEvents.h"
#include "LocalNode.h"
#include "MessageUtils.h"
#include "Messages.pb.h"
#include "Protocol.h"
#include "PythonClient.h"
#include "RemoteNode.h"
#include "ObjectRouter.h"
#include "ZUtils.h"

#include <boost/lexical_cast.hpp>
#include <boost/scope_exit.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/LockedArakoon.h>
#include <youtils/System.h>

#include <volumedriver/Api.h>
#include <volumedriver/ClusterLocation.h>
#include <volumedriver/ScrubWork.h>
#include <volumedriver/TransientException.h>
#include <volumedriver/VolManager.h>
#include <volumedriver/VolumeConfig.h>
#include <volumedriver/failovercache/fungilib/Mutex.h>

namespace volumedriverfs
{

using namespace std::literals::string_literals;

#define WLOCK_NODES()                                   \
    fungi::ScopedWriteLock nmlwg__(node_map_lock_)

#define RLOCK_NODES()                                   \
    fungi::ScopedReadLock nmlrg__(node_map_lock_)

#define LOCK_REDIRECTS()                                \
    std::lock_guard<decltype(redirects_lock_)> rlg__(redirects_lock_)

#define LOCK_FOC_CONFIG()                                               \
    std::lock_guard<decltype(foc_config_lock_)> fclg__(foc_config_lock_)

namespace ara = arakoon;
namespace be = backend;
namespace bpt = boost::property_tree;
namespace ip = initialized_params;
namespace vd = volumedriver;
namespace yt = youtils;

ObjectRouter::ObjectRouter(const bpt::ptree& pt,
                           std::shared_ptr<yt::LockedArakoon>(larakoon),
                           const FailOverCacheConfigMode foc_config_mode,
                           const vd::FailOverCacheMode foc_mode,
                           const MaybeFailOverCacheConfig& foc_config,
                           const RegisterComponent registrate)
    : VolumeDriverComponent(registrate,
                            pt)
    , node_map_lock_("vrouter-node-map-lock")
    , vrouter_volume_read_threshold(pt)
    , vrouter_volume_write_threshold(pt)
    , vrouter_file_read_threshold(pt)
    , vrouter_file_write_threshold(pt)
    , vrouter_check_local_volume_potential_period(pt)
    , vrouter_backend_sync_timeout_ms(pt)
    , vrouter_migrate_timeout_ms(pt)
    , vrouter_redirect_timeout_ms(pt)
    , vrouter_redirect_retries(pt)
    , vrouter_routing_retries(pt)
    , vrouter_id(pt)
    , vrouter_cluster_id(pt)
    , vrouter_min_workers(pt)
    , vrouter_max_workers(pt)
    , vrouter_registry_cache_capacity(pt)
    , vrouter_xmlrpc_client_timeout_ms(pt)
    , vrouter_use_fencing(pt)
    , vrouter_send_sync_response(pt)
    , larakoon_(larakoon)
    , object_registry_(std::make_shared<CachedObjectRegistry>(cluster_id(),
                                                              node_id(),
                                                              larakoon_,
                                                              vrouter_registry_cache_capacity.value()))
    , ztx_(new zmq::context_t(::sysconf(_SC_NPROCESSORS_ONLN)))
    , publisher_(std::make_shared<EventPublisher>(std::make_unique<AmqpEventPublisher>(cluster_id(),
                                                                                       node_id(),
                                                                                       pt)))
    , foc_config_mode_(foc_config_mode)
    , foc_mode_(foc_mode)
    , foc_config_(foc_config)
{
    LOG_TRACE("setting up");

    THROW_WHEN(vrouter_check_local_volume_potential_period.value() == 0);

    cluster_registry_ = std::make_shared<ClusterRegistry>(cluster_id(),
                                                          larakoon_);
    VERIFY(cluster_registry_);
    update_config_(pt);

    const ClusterNodeConfig ncfg(node_config());
    worker_pool_ = std::make_unique<ZWorkerPool>("ObjectRouterWorkerPool",
                                                 *ztx_,
                                                 ncfg.message_uri(),
                                                 vrouter_max_workers.value(),
                                                 [this](ZWorkerPool::MessageParts parts)
                                                 {
                                                     return redirected_work_(std::move(parts));
                                                 },
                                                 [](const ZWorkerPool::MessageParts&)
                                                 {
                                                     return yt::DeferExecution::T;
                                                 });
}

ObjectRouter::~ObjectRouter()
{
    shutdown_();
    LOG_INFO("Bye");
}

void
ObjectRouter::shutdown_()
{
    // ZMQ teardown
    // (1) destruct (close) all req sockets and all shared_ptrs to the ztx_
    {
        WLOCK_NODES();
        node_map_.clear();
    }

    // (2) this will close the ZMQ context and lead to ZWorkerPool shutting down
    //     synchronously.
    ztx_ = nullptr;
}

std::pair<ObjectRouter::NodeMap, ObjectRouter::ConfigMap>
ObjectRouter::build_config_(const boost::optional<const bpt::ptree&>& pt)
{
    const ClusterRegistry::NodeStatusMap status_map(get_node_status_map());
    NodeMap new_node_map;
    ConfigMap new_config_map;

    for (const auto& v : status_map)
    {
        const ClusterNodeConfig& cfg = v.second.config;
        std::shared_ptr<ClusterNode> old_node(find_node_(cfg.vrouter_id));
        if (old_node)
        {
            const auto it = config_map_.find(cfg.vrouter_id);
            VERIFY(it != config_map_.end());
            const ClusterNodeConfig& old_cfg = it->second;

#define NE(x)                                   \
            cfg.x != old_cfg.x

            if (NE(message_host) or
                NE(message_port) or
                NE(xmlrpc_host) or
                NE(xmlrpc_port) or
                NE(failovercache_host) or
                NE(failovercache_port) or
                NE(network_server_uri))
            {
                LOG_ERROR("Refusing to change the cluster config. Old: " <<
                          old_cfg << ", new: " << cfg);
                throw InvalidConfigurationException("Changing a cluster node's config is not supported",
                                                    cfg.vrouter_id.str().c_str());
            }
#undef NE
        }

        std::shared_ptr<ClusterNode> n;

        if (cfg.vrouter_id == node_id())
        {
            if (pt)
            {
                n = std::make_shared<LocalNode>(*this,
                                                cfg,
                                                *pt);

                const auto& state = v.second.state;
                if (state != ClusterNodeStatus::State::Online)
                {
                    LOG_WARN(node_id() <<
                             ": we were previously offlined. Setting ourselves online again");
                    cluster_registry_->set_node_state(node_id(),
                                                      ClusterNodeStatus::State::Online);
                }
            }
            else
            {
                VERIFY(old_node);
                n = old_node;
            }
        }
        else
        {
            n = std::make_shared<RemoteNode>(*this,
                                             cfg.vrouter_id,
                                             cfg.message_uri(),
                                             *ztx_);
        }

        bool ok = false;
        std::tie(std::ignore, ok) = new_node_map.emplace(cfg.vrouter_id, n);
        VERIFY(ok);
        std::tie(std::ignore, ok) = new_config_map.emplace(cfg.vrouter_id, cfg);
        VERIFY(ok);
    }

    auto it = new_node_map.find(node_id());
    if (it == new_node_map.end())
    {
        LOG_ERROR("Our very own node ID " << node_id() <<
                  " does not appear in the cluster nodes config");
        throw InvalidConfigurationException("Missing node ID in cluster config",
                                            node_id().str().c_str());
    }

    return std::make_pair(std::move(new_node_map), std::move(new_config_map));
}

void
ObjectRouter::update_config_(const boost::optional<const bpt::ptree&>& pt)
{
    NodeMap new_node_map;
    ConfigMap new_config_map;

    std::tie(new_node_map, new_config_map) = build_config_(pt);

    LOG_INFO("New ClusterNode configs:");

    for (const auto& p : new_config_map)
    {
        LOG_INFO("\t" << p.second);
    }

    WLOCK_NODES();

    std::swap(node_map_,
              new_node_map);
    std::swap(config_map_,
              new_config_map);
}

void
ObjectRouter::update_cluster_node_configs()
{
    update_config_(boost::none);
}

void
ObjectRouter::destroy(std::shared_ptr<yt::LockedArakoon> larakoon,
                      const bpt::ptree& pt)
{
    const ClusterId cluster_id(PARAMETER_VALUE_FROM_PROPERTY_TREE(vrouter_cluster_id, pt));
    const NodeId node_id(PARAMETER_VALUE_FROM_PROPERTY_TREE(vrouter_id, pt));

    ObjectRegistry reg(cluster_id,
                       node_id,
                       larakoon);

    LocalNode::destroy(reg,
                       pt);

    // We really only want this to run if all the other cleanups succeeded, otherwise
    // the objects will be invariably leaked. This way we can try again.
    reg.destroy();
}

ClusterNodeConfig
ObjectRouter::node_config() const
{
    auto maybe_cfg(node_config(node_id()));
    VERIFY(maybe_cfg);
    return *maybe_cfg;
}

boost::optional<ClusterNodeConfig>
ObjectRouter::node_config(const NodeId& node_id) const
{
    RLOCK_NODES();

    auto it = config_map_.find(node_id);
    if (it == config_map_.end())
    {
        return boost::none;
    }
    else
    {
        return it->second;
    }
}

namespace
{

template<typename M>
M
get_req(const ZWorkerPool::MessageParts& parts)
{
    M m;
    ZUtils::deserialize_from_message(parts[2], m);
    m.CheckInitialized();
    return m;
}

}

ZWorkerPool::MessageParts
ObjectRouter::redirected_work_(ZWorkerPool::MessageParts parts_in)
{
    // XXX: All this juggling with vector indices is pretty brittle. Slices would
    // be nice
    LOG_TRACE("got " << parts_in.size() << " message parts");

    // request layout:
    // * mandatory parts:
    // ** vfsprotocol::RequestType
    // ** vfsprotocol::Tag
    // ** RequestMessage
    // * optional data part(s) depending on RequestType

    // response layout:
    // * mandatory parts:
    // ** vfsprotocol::ResponseType
    // ** vfsprotocol::Tag
    // * optional ResponseMessage and / or data part(s) depending on ResponseType

    vfsprotocol::ResponseType rsp_type = vfsprotocol::ResponseType::Ok;
    vfsprotocol::Tag tag(0);

#define CHECK(cond)                                     \
    if (not (cond))                                     \
    {                                                   \
        LOG_ERROR("detected ProtocolError: " << #cond); \
        throw ProtocolError(#cond);                     \
    }

    ZWorkerPool::MessageParts parts_out;
    parts_out.reserve(3);

    try
    {
        CHECK(parts_in.size() >= 3);

        parts_out.emplace_back(ZUtils::serialize_to_message(rsp_type));

        vfsprotocol::RequestType req_type;
        ZUtils::deserialize_from_message(parts_in[0], req_type);

        // ATM we only get the tag out for verification purposes
        ZUtils::deserialize_from_message(parts_in[1], tag);
        parts_out.emplace_back(std::move(parts_in[1]));

        switch (req_type)
        {
        case vfsprotocol::RequestType::Ping:
            {
                CHECK(parts_in.size() == 3);
                parts_out.emplace_back(handle_ping_(get_req<vfsprotocol::PingMessage>(parts_in)));
                break;
            }
        case vfsprotocol::RequestType::Read:
            {
                CHECK(parts_in.size() == 3);
                parts_out.emplace_back(handle_read_(get_req<vfsprotocol::ReadRequest>(parts_in)));
                break;
            }
        case vfsprotocol::RequestType::Write:
            {
                CHECK(parts_in.size() == 4);
                parts_out.emplace_back(handle_write_(get_req<vfsprotocol::WriteRequest>(parts_in),
                                                     parts_in[3]));
                break;
            }
        case vfsprotocol::RequestType::Sync:
            {
                CHECK(parts_in.size() == 3);
                zmq::message_t msg(handle_sync_(get_req<vfsprotocol::SyncRequest>(parts_in)));
                if (vrouter_send_sync_response.value())
                {
                    parts_out.emplace_back(std::move(msg));
                }
                break;
            }
        case vfsprotocol::RequestType::GetSize:
            {
                CHECK(parts_in.size() == 3);
                parts_out.emplace_back(handle_get_size_(get_req<vfsprotocol::GetSizeRequest>(parts_in)));
                break;
            }
        case vfsprotocol::RequestType::GetClusterMultiplier:
            {
                CHECK(parts_in.size() == 3);
                parts_out.emplace_back(handle_get_cluster_multiplier_(get_req<vfsprotocol::GetClusterMultiplierRequest>(parts_in)));
                break;
            }
        case vfsprotocol::RequestType::GetCloneNamespaceMap:
            {
                CHECK(parts_in.size() == 3);
                parts_out.emplace_back(handle_get_clone_namespace_map_(get_req<vfsprotocol::GetCloneNamespaceMapRequest>(parts_in)));
                break;
            }
        case vfsprotocol::RequestType::GetPage:
            {
                CHECK(parts_in.size() == 3);
                parts_out.emplace_back(handle_get_page_(get_req<vfsprotocol::GetPageRequest>(parts_in)));
                break;
            }
        case vfsprotocol::RequestType::Resize:
            {
                CHECK(parts_in.size() == 3);
                handle_resize_(get_req<vfsprotocol::ResizeRequest>(parts_in));
                break;
            }
        case vfsprotocol::RequestType::Delete:
            {
                CHECK(parts_in.size() == 3);
                handle_delete_volume_(get_req<vfsprotocol::DeleteRequest>(parts_in));
                break;
            }
        case vfsprotocol::RequestType::Transfer:
            {
                CHECK(parts_in.size() == 3);
                handle_transfer_(get_req<vfsprotocol::TransferRequest>(parts_in));
                break;
            }
        default:
            LOG_ERROR("Got unexpected request type " << static_cast<uint32_t>(req_type));
            rsp_type = vfsprotocol::ResponseType::UnknownRequest;
            break;
        }
    }
    catch (vd::VolManager::VolumeDoesNotExistException&)
    {
        rsp_type = vfsprotocol::ResponseType::ObjectNotRunningHere;
    }
    catch (vd::AccessBeyondEndOfVolumeException&)
    {
        rsp_type = vfsprotocol::ResponseType::AccessBeyondEndOfVolume;
    }
    catch (vd::CannotShrinkVolumeException&)
    {
        rsp_type = vfsprotocol::ResponseType::CannotShrinkVolume;
    }
    catch (vd::CannotGrowVolumeBeyondLimitException&)
    {
        rsp_type = vfsprotocol::ResponseType::CannotGrowVolumeBeyondLimit;
    }
    catch (ObjectNotRunningHereException&)
    {
        rsp_type = vfsprotocol::ResponseType::ObjectNotRunningHere;
    }
    catch (ProtocolError&)
    {
        rsp_type = vfsprotocol::ResponseType::ProtocolError;
    }
    catch (TimeoutException&)
    {
        rsp_type = vfsprotocol::ResponseType::Timeout;
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Caught error while handling a message: " << EWHAT <<
                      " - dropping remaining message parts (if any) and building error response");
            // find/create a status code that is better suited
            rsp_type = vfsprotocol::ResponseType::IOError;
        });

    if (rsp_type != vfsprotocol::ResponseType::Ok)
    {
        parts_out.resize(2);
        parts_out[0] = ZUtils::serialize_to_message(rsp_type);
        parts_out[1] = ZUtils::serialize_to_message(tag);
    }

    LOG_TRACE("returning " << parts_out.size() << " message parts");
    return parts_out;

#undef CHECK
}

template<typename Ret,
         typename... Args>
Ret
ObjectRouter::maybe_steal_(Ret (ClusterNode::*fn)(const Object&,
                                                  Args...),
                           IsRemoteNode& remote,
                           AttemptTheft attempt_theft,
                           const ObjectRegistration& reg,
                           FastPathCookie& cookie,
                           Args&&... args)
{
    const NodeId& owner_id = reg.node_id;
    const ObjectId& id = reg.volume_id;
    const Object obj(reg.object());

    LOG_TRACE(id << ", owner " << owner_id);

    unsigned retry = 0;

    while (true)
    {
        std::shared_ptr<ClusterNode> node(find_node_or_throw_(owner_id));
        std::shared_ptr<LocalNode> lnode = std::dynamic_pointer_cast<LocalNode>(node);
        remote = lnode == nullptr ?
            IsRemoteNode::T :
            IsRemoteNode::F;

        try
        {
            cookie = lnode == nullptr ?
            nullptr :
            lnode->fast_path_cookie(obj);

            ClusterNode& cn = *node;
            return (cn.*fn)(obj, std::forward<Args>(args)...);
        }
        catch (RequestTimeoutException&)
        {
            VERIFY(remote == IsRemoteNode::T);
            LOG_ERROR(id << ": remote node " << owner_id << " timed out");

            if (cluster_registry_->get_node_status(owner_id).state ==
                ClusterNodeStatus::State::Online)
            {
                const auto ev(FileSystemEvents::redirect_timeout_while_online(owner_id));
                publisher_->publish(ev);
            }

            if (attempt_theft == AttemptTheft::F)
            {
                LOG_TRACE("Not even attempting to steal object " << id <<
                          " from " << owner_id);
                throw;
            }

            const bool permit_steal = permit_steal_(id);
            const bool stolen = steal_(reg,
                                       permit_steal ?
                                       OnlyStealFromOfflineNode::F :
                                       OnlyStealFromOfflineNode::T,
                                       permit_steal ?
                                       ForceRestart::F :
                                       ForceRestart::T);
            if (not stolen)
            {
                LOG_ERROR("Failed to steal " << id << " from " << owner_id <<
                          "(attempt " << retry << ")");

                if (++retry > vrouter_redirect_retries.value())
                {
                    throw;
                }
                else
                {
                    LOG_TRACE("Node " << owner_id << ", object " << id <<
                              ": making another attempt");
                }
            }
            else
            {
                remote = IsRemoteNode::F;

                LOG_INFO(reg.volume_id << ": retrying I/O after stealing");

                ClusterNode& cn = *local_node_();
                return (cn.*fn)(reg.object(), args...);
            }
        }
    }
}

std::shared_ptr<ClusterNode>
ObjectRouter::find_node_(const NodeId& node_id) const
{
    RLOCK_NODES();

    auto it = node_map_.find(node_id);
    if (it == node_map_.end())
    {
        return nullptr;
    }
    else
    {
        return it->second;
    }
}

std::shared_ptr<ClusterNode>
ObjectRouter::find_node_or_throw_(const NodeId& node_id) const
{
    auto n(find_node_(node_id));
    if (n == nullptr)
    {
        LOG_FATAL("Cannot find node " << node_id << " in the local config");
        throw InvalidConfigurationException("Cannot find node",
                                            node_id.str().c_str());
    }

    return n;
}

std::shared_ptr<LocalNode>
ObjectRouter::local_node_() const
{
    std::shared_ptr<ClusterNode> cn(find_node_or_throw_(node_id()));
    std::shared_ptr<LocalNode> ln = std::dynamic_pointer_cast<LocalNode>(cn);
    VERIFY(ln);
    return ln;
}

void
ObjectRouter::backend_restart_(const Object& obj,
                               ForceRestart force,
                               PrepareRestartFun prep_restart_fun)
{
    local_node_()->backend_restart(obj,
                                   force,
                                   std::move(prep_restart_fun));
    LOCK_REDIRECTS();
    redirects_.erase(obj.id);
}

bool
ObjectRouter::steal_(const ObjectRegistration& reg,
                     OnlyStealFromOfflineNode only_steal_if_offline,
                     ForceRestart force_restart)
{
    LOG_INFO("Checking whether we should steal " << reg.volume_id << " from " <<
             reg.node_id << ", only steal if offline: " << only_steal_if_offline <<
             ", force restart: " << force_restart);
    VERIFY(reg.node_id != node_id());

    auto fun([&](ara::sequence& seq)
             {
                 if (only_steal_if_offline == OnlyStealFromOfflineNode::T)
                 {
                     cluster_registry_->prepare_node_offline_assertion(seq,
                                                                       reg.node_id);
                 }
                 object_registry_->prepare_migrate(seq,
                                                   reg.volume_id,
                                                   reg.node_id,
                                                   node_id());
             });

    try
    {
        larakoon_->run_sequence("steal volume",
                                std::move(fun),
                                yt::RetryOnArakoonAssert::T);

        // XXX: try to push this into the CachedObjectRegistry
        object_registry_->drop_entry_from_cache(reg.volume_id);

        LOG_INFO("registry updated, we're now owner of " << reg.volume_id);

        event_publisher()->publish(FileSystemEvents::owner_changed(reg.volume_id,
                                                                   node_id()));
    }
    catch (ClusterNodeNotOfflineException&)
    {
        return false;
    }

    try
    {
        backend_restart_(reg.object(),
                         force_restart,
                         [](const Object&){});
        LOG_INFO(reg.volume_id << ": successfully stolen from " << reg.node_id);
        return true;
    }
    CATCH_STD_ALL_LOG_RETHROW(reg.volume_id << ": failed to restart");
}

namespace
{

void
maybe_take_a_nap(uint32_t attempt)
{
    // put a 0 here in the beginning and lose the 1 based vs. 0 based.
    static const std::vector<uint32_t> table{ 100, 200, 400, 800, 1600, 3200, 6400, 12800, 25600, 51200, 102400 };

    if (attempt > 0)
    {
        const uint32_t idx = (attempt >= table.size()) ?
            (table.size() - 1) :
            (attempt - 1);

        std::this_thread::sleep_for(std::chrono::microseconds(table[idx]));
    }
}

}

template<typename Ret,
         typename... Args>
Ret
ObjectRouter::do_route_(Ret (ClusterNode::*fn)(const Object&,
                                               Args...),
                        IsRemoteNode& remote,
                        AttemptTheft attempt_theft,
                        const ObjectId& id,
                        FastPathCookie& cookie,
                        Args&&... args)
{
    LOG_TRACE(id);

    ObjectRegistrationPtr reg(object_registry_->find_throw(id,
                                                           IgnoreCache::F));

    return do_route_(fn,
                     remote,
                     attempt_theft,
                     reg,
                     cookie,
                     std::forward<Args>(args)...);
}

template<typename Ret,
         typename... Args>
Ret
ObjectRouter::do_route_(Ret (ClusterNode::*fn)(const Object&,
                                               Args...),
                        IsRemoteNode& remote,
                        AttemptTheft attempt_theft,
                        ObjectRegistrationPtr reg,
                        FastPathCookie& cookie,
                        Args&&... args)
{
    const ObjectId& id = reg->volume_id;

    LOG_TRACE(id << ": probably owned by " << reg->node_id);

    for (uint32_t attempt = 0; attempt < (1 + vrouter_routing_retries.value()); ++attempt)
    {
        LOG_TRACE(id << ": routing attempt " << attempt);

        if (attempt > 0)
        {
            reg = object_registry_->find_throw(id,
                                               IgnoreCache::T);
        }

        LOG_TRACE(id << ": purportedly hosted by " << reg->node_id);

        try
        {
            return maybe_steal_(fn,
                                remote,
                                attempt_theft,
                                *reg,
                                cookie,
                                std::forward<Args>(args)...);
        }
        catch (vd::VolManager::VolumeDoesNotExistException&)
        {
            LOG_TRACE(id << ": not found on " << reg->node_id);
        }
        catch (ObjectNotRunningHereException&)
        {
            LOG_TRACE(id << ": not found on " << reg->node_id);
        }
        catch (WrongOwnerException&)
        {
            LOG_TRACE(id << ": not owned by " << reg->node_id);
        }

        LOG_TRACE(id << ": routing attempt " << attempt <<
                  " failed b/c the volume could (temporarily?) not be found");

        maybe_take_a_nap(attempt);
    }

    LOG_ERROR(id << ": volume location not found, giving up");
    throw vd::VolManager::VolumeDoesNotExistException("Volume location not found",
                                                      id.str().c_str());
}

template<typename Ret, typename... Args>
Ret
ObjectRouter::route_(Ret (ClusterNode::*fn)(const Object&,
                                            Args...),
                     AttemptTheft attempt_theft,
                     const ObjectId& id,
                     FastPathCookie& cookie,
                     Args... args)
{
    IsRemoteNode remote = IsRemoteNode::F;
    return do_route_(fn,
                     remote,
                     attempt_theft,
                     id,
                     cookie,
                     std::forward<Args>(args)...);
}

// TODO: It'd be nice to make this work with arbitrary return types.
template<typename Pred,
         typename... Args>
FastPathCookie
ObjectRouter::maybe_migrate_(Pred&& migrate_pred,
                             void (ClusterNode::*fn)(const Object&,
                                                     Args...),
                             const ObjectId& id,
                             Args... args)
{
    ObjectRegistrationPtr reg(object_registry_->find_throw(id,
                                                           IgnoreCache::F));

    IsRemoteNode remote = IsRemoteNode::F;
    FastPathCookie cookie;

    do_route_(fn,
              remote,
              AttemptTheft::T,
              reg,
              cookie,
              std::forward<Args>(args)...);

    if (remote == IsRemoteNode::T and migrate_pred(reg->volume_id,
                                                   reg->treeconfig.object_type))
    {
        LOG_INFO(id << ": investigating auto migration");

        // Don't reuse the previously retrieved ObjectRegistration as it was cached
        // and possibly outdated or could have changed in the mean time.
        // Don't bother with possibly outdated registrations in the cache.
        reg = object_registry_->find_throw(id,
                                           IgnoreCache::T);
        if (reg->node_id == node_id())
        {
            LOG_INFO(id <<
                     ": already migrated here while we were trying remote");
        }
        else
        {
            LOG_INFO(id << ": attempting auto migration from " << reg->node_id);

            try
            {
                // ForceRestart::T: it's ok to ignore the FOC in this case (and
                // ForceRestart should probably be renamed to make its semantics
                // clear(er)) as the remote first has to write out all pending data
                // to the backend before we do a restart here, so the FOC will be
                // empty anyway.
                const bool permit_steal = permit_steal_(id);
                migrate_(*reg,
                         permit_steal ?
                         OnlyStealFromOfflineNode::F :
                         OnlyStealFromOfflineNode::T,
                         permit_steal ?
                         ForceRestart::F :
                         ForceRestart::T);
                LOG_INFO(id << ": auto migration from " << reg->node_id << " done");
            }
            catch (RemoteTimeoutException&)
            {
                LOG_WARN(id << ": remote node reported timeout");
            }
            CATCH_STD_ALL_EWHAT({
                    reg = object_registry_->find(id,
                                                 IgnoreCache::T);
                    if (reg and reg->node_id == node_id())
                    {
                        LOG_INFO(id <<
                                 ": already migrated here while we were trying to do that ourselves");
                    }
                    else
                    {
                        LOG_WARN("Failed to automigrate " <<
                                 id << " from " <<
                                 (reg ?
                                  reg->node_id :
                                  NodeId("(unknown source)")) <<
                                  ": " << EWHAT);
                    }
                });
        }
    }

    return cookie;
}

zmq::message_t
ObjectRouter::handle_ping_(const vfsprotocol::PingMessage& req)
{
    LOG_TRACE("got ping from " << req.sender_id());
    const auto rsp(vfsprotocol::MessageUtils::create_ping_message(node_id()));
    return ZUtils::serialize_to_message(rsp);
}

void
ObjectRouter::ping(const NodeId& node)
{
    LOG_TRACE(node);

    if (node == node_id())
    {
        LOG_WARN("Not pinging ourselves");
    }
    else
    {
        auto remote(std::dynamic_pointer_cast<RemoteNode>(find_node_or_throw_(node)));

        LOG_TRACE(node << ": ping");
        remote->ping();
        LOG_TRACE(node << ": pong");
    }
}

bool
ObjectRouter::migrate_pred_helper_(const char* desc,
                                   const ObjectId& id,
                                   bool is_volume,
                                   uint64_t thresh,
                                   uint64_t& counter) const
{
    ++counter;

    LOG_TRACE(id << ": " <<
              (is_volume ? "volume" : "file") << " " <<
              desc << " threshold: " << thresh <<
              ", redirects so far: " << counter);

    if (thresh > 0 and counter >= thresh)
    {
        if (is_volume)
        {
            const uint64_t p = vrouter_check_local_volume_potential_period.value();
            VERIFY(p);

            if (((counter - thresh) % p) == 0)
            {
                LOG_INFO(id << ": checking volume potential of local node");

                try
                {
                    return local_node_()->volume_potential(id) > 0;
                }
                CATCH_STD_ALL_EWHAT({
                        LOG_ERROR(id <<
                                  ": failed to determine volume potential of local node: " <<
                                  EWHAT << " - let's rather not migrate here");
                        return false;
                    });
            }
            else
            {
                LOG_TRACE(id << ": not checking volume potential of local node");
                return false;
            }
        }
        else
        {
            return true;
        }
    }
    else
    {
        return false;
    }
}

template<typename... Args>
FastPathCookie
ObjectRouter::select_path_(const FastPathCookie& cookie,
                           FastPathCookie (ObjectRouter::*slow_fun)(Args...),
                           FastPathCookie (LocalNode::*fast_fun)(const FastPathCookie&,
                                                                 Args...),
                           Args... args)
{
    if (cookie)
    {
        try
        {
            LocalNode& node = *local_node_();
            return (node.*fast_fun)(cookie,
                                    args...);
        }
        catch (ObjectNotRunningHereException&)
        {}
    }

    return (*this.*slow_fun)(std::forward<Args>(args)...);
}

FastPathCookie
ObjectRouter::write(const FastPathCookie& cookie,
                    const ObjectId& id,
                    const uint8_t* buf,
                    size_t size,
                    off_t off,
                    vd::DtlInSync& dtl_in_sync)
{
    using FastPathFun = FastPathCookie (LocalNode::*)(const FastPathCookie&,
                                                      const ObjectId&,
                                                      const uint8_t*,
                                                      size_t*,
                                                      off_t,
                                                      vd::DtlInSync&);

    return select_path_<decltype(id),
                        decltype(buf),
                        size_t*,
                        decltype(off),
                        decltype(dtl_in_sync)>(cookie,
                                               &ObjectRouter::write_,
                                               static_cast<FastPathFun>(&LocalNode::write),
                                               id,
                                               buf,
                                               &size,
                                               off,
                                               dtl_in_sync);
}

FastPathCookie
ObjectRouter::write_(const ObjectId& id,
                     const uint8_t* buf,
                     size_t* size,
                     off_t off,
                     vd::DtlInSync& dtl_in_sync)
{
    LOG_TRACE(id << ": size " << *size << ", off " << off);

    auto pred([this, &dtl_in_sync](const ObjectId& id,
                                   const ObjectType tp)
              {
                  LOCK_REDIRECTS();

                  RedirectCounter& counter = redirects_[id];
                  counter.dtl_in_sync = dtl_in_sync;

                  return migrate_pred_helper_("write",
                                              id,
                                              tp != ObjectType::File,
                                              tp == ObjectType::File ?
                                              vrouter_file_write_threshold.value() :
                                              vrouter_volume_write_threshold.value(),
                                              counter.writes);
              });

    using WriteFun = void (ClusterNode::*)(const Object&,
                                           const uint8_t*,
                                           size_t*,
                                           const off_t,
                                           vd::DtlInSync&);

    return maybe_migrate_<decltype(pred),
                          decltype(buf),
                          decltype(size),
                          decltype(off),
                          vd::DtlInSync&>(std::move(pred),
                                          static_cast<WriteFun>(&ClusterNode::write),
                                          id,
                                          buf,
                                          size,
                                          off,
                                          dtl_in_sync);
}

namespace
{

template<typename M>
Object
obj_from_msg(const M& msg)
{
    TODO("AR: use a protobufs enum");
    return Object(static_cast<ObjectType>(msg.object_type()),
                  ObjectId(msg.object_id()));
}

}

zmq::message_t
ObjectRouter::handle_write_(const vfsprotocol::WriteRequest& req,
                            const zmq::message_t& data)
{
    const Object obj(obj_from_msg(req));

    LOG_TRACE(obj << ": size " << req.size() << ", off " << req.offset());

    vd::DtlInSync dtl_in_sync = vd::DtlInSync::F;

    size_t size = req.size();
    local_node_()->write(obj,
                         reinterpret_cast<const uint8_t*>(data.data()),
                         &size,
                         req.offset(),
                         dtl_in_sync);

    const auto rsp(vfsprotocol::MessageUtils::create_write_response(size, dtl_in_sync));
    return ZUtils::serialize_to_message(rsp);
}

FastPathCookie
ObjectRouter::read(const FastPathCookie& cookie,
                   const ObjectId& id,
                   uint8_t* buf,
                   size_t& size,
                   off_t off)
{
    return select_path_<decltype(id),
                        decltype(buf),
                        size_t*,
                        decltype(off)>(cookie,
                                       &ObjectRouter::read_,
                                       &LocalNode::read,
                                       id,
                                       buf,
                                       &size,
                                       off);
}

FastPathCookie
ObjectRouter::read_(const ObjectId& id,
                    uint8_t* buf,
                    size_t* size,
                    off_t off)
{
    LOG_TRACE(id << ": size " << size << ", off " << off);

    auto pred([this](const ObjectId& id,
                     const ObjectType tp)
              {
                  LOCK_REDIRECTS();

                  return migrate_pred_helper_("read",
                                              id,
                                              tp != ObjectType::File,
                                              tp == ObjectType::File ?
                                              vrouter_file_read_threshold.value() :
                                              vrouter_volume_read_threshold.value(),
                                              redirects_[id].reads);
              });

    return maybe_migrate_(std::move(pred),
                          &ClusterNode::read,
                          id,
                          buf,
                          size,
                          off);
}

namespace
{

void
delete_read_buf(void* data, void* /* hint */)
{
    uint8_t* rbuf = static_cast<uint8_t*>(data);
    ASSERT(rbuf != nullptr);
    delete[] rbuf;
}

}

zmq::message_t
ObjectRouter::handle_read_(const vfsprotocol::ReadRequest& req)
{
    size_t size = req.size();

    std::unique_ptr<uint8_t[]> rbuf(new uint8_t[size]);

    const Object obj(obj_from_msg(req));

    LOG_TRACE(obj << ": size " << size << ", off " << req.offset());

    local_node_()->read(obj,
                        rbuf.get(),
                        &size,
                        req.offset());

    zmq::message_t data(rbuf.get(), size, delete_read_buf);
    rbuf.release();

    return data;
}

FastPathCookie
ObjectRouter::sync(const FastPathCookie& cookie,
                   const ObjectId& id,
                   vd::DtlInSync& dtl_in_sync)
{
    FastPathCookie
        fpc(select_path_<decltype(id),
                         decltype(dtl_in_sync)>(cookie,
                                                &ObjectRouter::sync_,
                                                &LocalNode::sync,
                                                id,
                                                dtl_in_sync));
    if (not fpc)
    {
        LOCK_REDIRECTS();
        redirects_[id].dtl_in_sync = dtl_in_sync;
    }

    return fpc;
}

FastPathCookie
ObjectRouter::sync_(const ObjectId& id,
                    vd::DtlInSync& dtl_in_sync)
{
    LOG_TRACE(id);

    FastPathCookie cookie;

    route_<void,
           decltype(dtl_in_sync)>(&ClusterNode::sync,
                                  AttemptTheft::T,
                                  id,
                                  cookie,
                                  dtl_in_sync);

    return cookie;
}

zmq::message_t
ObjectRouter::handle_sync_(const vfsprotocol::SyncRequest& req)
{
    const Object obj(obj_from_msg(req));
    LOG_TRACE(obj);
    vd::DtlInSync dtl_in_sync = vd::DtlInSync::F;
    local_node_()->sync(obj,
                        dtl_in_sync);
    const auto rsp(vfsprotocol::MessageUtils::create_sync_response(dtl_in_sync));
    return ZUtils::serialize_to_message(rsp);
}

uint64_t
ObjectRouter::get_size(const ObjectId& id)
{
    LOG_TRACE(id);

    FastPathCookie cookie;

    return route_(&ClusterNode::get_size,
                  AttemptTheft::T,
                  id,
                  cookie);
}

zmq::message_t
ObjectRouter::handle_get_size_(const vfsprotocol::GetSizeRequest& msg)
{
    const Object obj(obj_from_msg(msg));

    LOG_TRACE(obj);
    const uint64_t size = local_node_()->get_size(obj);
    const auto rsp(vfsprotocol::MessageUtils::create_get_size_response(size));
    return ZUtils::serialize_to_message(rsp);
}

vd::ClusterMultiplier
ObjectRouter::get_cluster_multiplier(const ObjectId& id)
{
    LOG_TRACE(id);

    FastPathCookie cookie;

    return route_(&ClusterNode::get_cluster_multiplier,
                  AttemptTheft::T,
                  id,
                  cookie);
}

zmq::message_t
ObjectRouter::handle_get_cluster_multiplier_(const vfsprotocol::GetClusterMultiplierRequest& msg)
{
    const Object obj(obj_from_msg(msg));

    LOG_TRACE(obj);
    const vd::ClusterMultiplier cm =
        local_node_()->get_cluster_multiplier(obj);
    const auto rsp(vfsprotocol::MessageUtils::create_get_cluster_multiplier_response(cm));
    return ZUtils::serialize_to_message(rsp);
}

vd::CloneNamespaceMap
ObjectRouter::get_clone_namespace_map(const ObjectId& id)
{
    LOG_TRACE(id);

    FastPathCookie cookie;

    return route_(&ClusterNode::get_clone_namespace_map,
                  AttemptTheft::T,
                  id,
                  cookie);
}

zmq::message_t
ObjectRouter::handle_get_clone_namespace_map_(const vfsprotocol::GetCloneNamespaceMapRequest& msg)
{
    const Object obj(obj_from_msg(msg));

    LOG_TRACE(obj);
    const vd::CloneNamespaceMap cnmap =
        local_node_()->get_clone_namespace_map(obj);
    const auto rsp(vfsprotocol::MessageUtils::create_get_clone_namespace_map_response(cnmap));
    return ZUtils::serialize_to_message(rsp);
}

std::vector<vd::ClusterLocation>
ObjectRouter::get_page(const ObjectId& id,
                       const vd::ClusterAddress ca)
{
    LOG_TRACE(id);

    FastPathCookie cookie;

    return route_(&ClusterNode::get_page,
                  AttemptTheft::T,
                  id,
                  cookie,
                  ca);
}

zmq::message_t
ObjectRouter::handle_get_page_(const vfsprotocol::GetPageRequest& msg)
{
    const Object obj(obj_from_msg(msg));

    LOG_TRACE(obj);
    const std::vector<vd::ClusterLocation> cl =
        local_node_()->get_page(obj,
                                vd::ClusterAddress(msg.cluster_address()));
    const auto rsp(vfsprotocol::MessageUtils::create_get_page_response(cl));
    return ZUtils::serialize_to_message(rsp);
}

void
ObjectRouter::resize(const ObjectId& id, uint64_t newsize)
{
    LOG_TRACE(id << ", newsize " << newsize);

    FastPathCookie cookie;

    route_(&ClusterNode::resize,
           AttemptTheft::T,
           id,
           cookie,
           newsize);
}

void
ObjectRouter::handle_resize_(const vfsprotocol::ResizeRequest& req)
{
    const Object obj(obj_from_msg(req));
    const uint64_t size = req.size();

    LOG_TRACE(obj << ": new size " << size);
    local_node_()->resize(obj, size);
}

void
ObjectRouter::unlink(const ObjectId& id)
{
    LOG_INFO(id);

    // Unregister is done by the localnode, before the destroy,
    // otherwise a concurrent call to create a clone from a
    // template currently being deleted could succeed and lead to dataloss.

    FastPathCookie cookie;

    try
    {
        route_(&ClusterNode::unlink,
               AttemptTheft::F,
               id,
               cookie);
    }
    catch (ObjectNotRegisteredException&)
    {
        // Swallow this one. This is slightly tricky (which points to the fact that
        // it should be redone to make it more explicit) as it deals with both
        // (a) the volume being gone but the registration still being around, and
        // (b) the registration not being there:
        //
        // In case (a)
        // 1) do_route_ will ask the owner to delete the volume, which will
        // 2) unregister the volume and then
        // 3) try to remove it from the volumedriver. This will however throw,
        //    which will lead to a
        // 4) retry in do_route_ which in turn will then
        // 5) throw a ObjectNotRegisteredException
        //
        // In case (b) we'll obviously get the ObjectNotRegisteredException right away
        // - if the volume does indeed exist at volumedriver level it's leaked, but there's
        // not much we can do about it at the moment.

        LOG_WARN("Volume " << id << " is not registered (anymore)");
    }
}

void
ObjectRouter::handle_delete_volume_(const vfsprotocol::DeleteRequest& req)
{
    const Object obj(obj_from_msg(req));

    LOG_TRACE(obj);
    local_node_()->unlink(obj);
}

void
ObjectRouter::handle_transfer_(const vfsprotocol::TransferRequest& req)
{
    const Object obj(obj_from_msg(req));
    const NodeId target_node(req.target_node_id());
    LocalNode::MaybeSyncTimeoutMilliSeconds maybe_sync_timeout_ms;

    if (req.has_sync_timeout_ms() and req.sync_timeout_ms())
    {
        maybe_sync_timeout_ms = boost::chrono::milliseconds(req.sync_timeout_ms());
    }

    LOG_TRACE(obj << ": transferring to " << target_node);
    local_node_()->transfer(obj,
                            target_node,
                            maybe_sync_timeout_ms);
}

void
ObjectRouter::migrate_(const ObjectRegistration& reg,
                       OnlyStealFromOfflineNode only_steal_if_offline,
                       ForceRestart force)
{
    const NodeId& from = reg.node_id;
    const ObjectId& id = reg.volume_id;

    LOG_INFO("Trying to migrate " << id << " from " << from <<
             ", only steal if offline " << only_steal_if_offline);

    if (from == node_id())
    {
        local_node_()->backend_restart(reg.object(),
                                       force,
                                       [](const Object&){});
    }
    else
    {
        auto remote_node(std::dynamic_pointer_cast<RemoteNode>(find_node_or_throw_(from)));

        try
        {
            const Object obj(reg.object());

            backend_restart_(obj,
                             force,
                             [&](const Object& o)
                             {
                                 remote_node->transfer(o);
                             });

            LOG_INFO(obj << " successfully migrated from " << from);
        }
        catch (RequestTimeoutException&)
        {
            LOG_ERROR(id << ": remote node " << from << " timed out while migrating");
            if (not steal_(reg,
                           only_steal_if_offline,
                           force))
            {
                throw;
            }
        }
    }
}

// Volumes are *always* created locally - no need to try at the remote site
void
ObjectRouter::create(const Object& obj,
                     vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config)
{
    LOG_INFO("Creating " << obj);
    local_node_()->create(obj,
                          std::move(mdb_config));
}

void
ObjectRouter::create_clone(const vd::VolumeId& clone_id,
                           vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config,
                           const vd::VolumeId& parent_id,
                           const MaybeSnapshotName& maybe_parent_snap)
{
    LOG_INFO("cloning volume " << clone_id << " from parent " << parent_id <<
             ", snapshot " << maybe_parent_snap);

    TODO("AR: revisit conversion to ObjectId at this point");
    local_node_()->create_clone(ObjectId(clone_id.str()),
                                std::move(mdb_config),
                                ObjectId(parent_id.str()),
                                maybe_parent_snap);
}
void
ObjectRouter::clone_to_existing_volume(const vd::VolumeId& clone_id,
                                       vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config,
                                       const vd::VolumeId& parent_id,
                                       const MaybeSnapshotName& maybe_parent_snap)
{
    LOG_INFO("cloning to existing volume " << clone_id << " from parent " << parent_id <<
             ", snapshot " << maybe_parent_snap);

    local_node_()->clone_to_existing_volume(ObjectId(clone_id.str()),
                                            std::move(mdb_config),
                                            ObjectId(parent_id.str()),
                                            maybe_parent_snap);
}

void
ObjectRouter::vaai_copy(const ObjectId& src_id,
                        const boost::optional<ObjectId>& maybe_dst_id,
                        std::unique_ptr<vd::MetaDataBackendConfig> mdb_config,
                        const uint64_t timeout,
                        const CloneFileFlags flags,
                        VAAICreateCloneFun fun)
{

    LOG_INFO("copying file " << src_id << " to " << maybe_dst_id);
    local_node_()->vaai_copy(src_id,
                             maybe_dst_id,
                             std::move(mdb_config),
                             timeout,
                             flags,
                             fun);
}

void
ObjectRouter::vaai_filecopy(const ObjectId& src_id,
                            const ObjectId& dst_id)
{
    LOG_INFO("copying file " << src_id << " to " << dst_id);
    local_node_()->vaai_filecopy(src_id,
                                 dst_id);
}

void
ObjectRouter::migrate(const ObjectId& id,
                      ForceRestart force)
{
    LOG_INFO("Migrating " << id);

    ObjectRegistrationPtr reg(object_registry_->find_throw(id,
                                                           IgnoreCache::T));
    migrate_(*reg,
             force == ForceRestart::T ?
             OnlyStealFromOfflineNode::F :
             OnlyStealFromOfflineNode::T,
             force);
}

void
ObjectRouter::stop(const ObjectId& id,
                   vd::DeleteLocalData delete_local_data,
                   const CheckOwner check_owner)
{
    LOG_INFO("Attempting to stop " << id);

    ObjectRegistrationPtr reg(object_registry_->find_throw(id,
                                                           IgnoreCache::T));

    if (check_owner == CheckOwner::T and reg->node_id != node_id())
    {
        LOG_ERROR(id << " is not running here (" << node_id() << ") but on node " <<
                  reg->node_id);
        throw ObjectNotRunningHereException("Volume not running here",
                                            id.str().c_str());
    }

    local_node_()->stop(reg->object(),
                        delete_local_data);
}

bool
ObjectRouter::maybe_restart(const ObjectId& id,
                            ForceRestart force)
{
    // XXX: push the whole thing down to LocalNode?
    //
    //We only throw away data with "removeLocalVolumeData" when
    // - the volume is known in the registry
    // - it's assigned to another node
    // Otherwize we could have dataloss due to a misconfiguration of e.g. arakoon on startup and no volumes
    // being found.


    const ObjectRegistrationPtr reg(object_registry_->find_throw(id,
                                                                 IgnoreCache::T));
    if (reg->node_id != node_id())
    {
        LOG_INFO("Not restarting " << id << " here as it's running on " << reg->node_id);
        local_node_()->remove_local_data(reg->object());
        return false;
    }
    else
    {
        local_node_()->local_restart(*reg,
                                     force);
        return true;
    }
}

void
ObjectRouter::restart(const ObjectId& id,
                      ForceRestart force)
{
    const bool res = maybe_restart(id,
                                   force);
    if (not res)
    {
        throw ObjectNotRunningHereException("object is not owned by this node",
                                            id.str().c_str());
    }
}

void
ObjectRouter::set_volume_as_template_local(const vd::VolumeId& id)
{
    LOG_INFO("Set volume " << id << " as template");
    TODO("AR: revisit conversion to ObjectId at this point");
    local_node_()->set_volume_as_template(ObjectId(id.str()));
}

void
ObjectRouter::create_snapshot_local(const ObjectId& volume_id,
                                    const vd::SnapshotName& snap_id,
                                    const int64_t timeout)
{
    LOG_INFO("Snapshotting volume '" << volume_id << "' with snapshot name '"
             << snap_id << "'");
    local_node_()->create_snapshot(volume_id,
                                   snap_id,
                                   timeout);
}

void
ObjectRouter::create_snapshot(const ObjectId& volume_id,
                              const vd::SnapshotName& snap_id,
                              const int64_t timeout)
{
    LOG_INFO("Snapshotting volume '" << volume_id << "' with snapshot name '"
             << snap_id << "'");
    std::unique_ptr<PythonClient> client(xmlrpc_client());

    client->create_snapshot(volume_id.str(),
                            snap_id.str());

    if (timeout >= 0)
    {
        const auto end = boost::chrono::steady_clock::now() + boost::chrono::seconds(timeout);
        bool synced = false;

        do
        {
            synced = client->is_volume_synced_up_to_snapshot(volume_id.str(),
                                                             snap_id.str());
            if (not synced)
            {
                boost::this_thread::sleep_for(boost::chrono::milliseconds(250));
            }
        }
        while (not synced and (timeout == 0 or boost::chrono::steady_clock::now() < end));

        if (not synced)
        {
            client->delete_snapshot(volume_id.str(),
                                    snap_id.str());
            throw SyncTimeoutException("timeout syncing snapshot to backend\n");
        }
    }
}

void
ObjectRouter::rollback_volume_local(const ObjectId& volume_id,
                                    const vd::SnapshotName& snap_id)
{
    LOG_INFO("Rolling back " << volume_id << " to snapshot " << snap_id);

    local_node_()->rollback_volume(volume_id,
                                   snap_id);
}

void
ObjectRouter::rollback_volume(const ObjectId& volume_id,
                              const vd::SnapshotName& snap_id)
{
    LOG_INFO("Rolling back " << volume_id << " to snapshot " << snap_id);

    xmlrpc_client()->rollback_volume(volume_id.str(),
                                     snap_id.str());
}

void
ObjectRouter::delete_snapshot_local(const ObjectId& oid,
                                    const vd::SnapshotName& snap)
{
    LOG_INFO("Deleting snapshot " << snap << " from " << oid);
    local_node_()->delete_snapshot(oid,
                                   snap);
}

void
ObjectRouter::delete_snapshot(const ObjectId& oid,
                              const vd::SnapshotName& snap)
{
    LOG_INFO("Deleting snapshot " << snap << " from " << oid);
    xmlrpc_client()->delete_snapshot(oid.str(),
                                     snap.str());
}

std::list<vd::SnapshotName>
ObjectRouter::list_snapshots_local(const ObjectId& oid)
{
    LOG_INFO("Listing snapshots for volume '" << oid << "'");
    return local_node_()->list_snapshots(oid);
}

std::vector<std::string>
ObjectRouter::list_snapshots(const ObjectId& oid)
{
    return xmlrpc_client()->list_snapshots(oid.str());
}

bool
ObjectRouter::is_volume_synced_up_to_local(const ObjectId& id,
                                           const vd::SnapshotName& snap_id)
{
    return local_node_()->is_volume_synced_up_to(id,
                                                 snap_id);
}

bool
ObjectRouter::is_volume_synced_up_to(const ObjectId& id,
                                     const vd::SnapshotName& snap_id)
{
    return xmlrpc_client()->is_volume_synced_up_to_snapshot(id.str(),
                                                            snap_id.str());
}

std::vector<scrubbing::ScrubWork>
ObjectRouter::get_scrub_work_local(const ObjectId& oid,
                                   const boost::optional<vd::SnapshotName>& start_snap,
                                   const boost::optional<vd::SnapshotName>& end_snap)
{
    LOG_INFO(oid << ": getting scrub work, start snapshot " << start_snap <<
             ", end snapshot " << end_snap);
    return local_node_()->get_scrub_work(oid,
                                         start_snap,
                                         end_snap);
}

void
ObjectRouter::queue_scrub_reply_local(const ObjectId& oid,
                                      const scrubbing::ScrubReply& scrub_reply)
{
    LOG_INFO(oid << ": queuing scrub reply");
    return local_node_()->queue_scrub_reply(oid,
                                            scrub_reply);
}

const char*
ObjectRouter::componentName() const
{
    return ip::volumerouter_component_name;
}

void
ObjectRouter::update(const bpt::ptree& pt,
                     yt::UpdateReport& rep)
{
#define U(val)                                  \
    val.update(pt, rep)

    U(vrouter_volume_read_threshold);
    U(vrouter_volume_write_threshold);
    U(vrouter_file_read_threshold);
    U(vrouter_file_write_threshold);
    U(vrouter_check_local_volume_potential_period);
    U(vrouter_backend_sync_timeout_ms);
    U(vrouter_migrate_timeout_ms);
    U(vrouter_redirect_timeout_ms);
    U(vrouter_routing_retries);
    U(vrouter_redirect_retries);
    U(vrouter_id);
    U(vrouter_cluster_id);
    U(vrouter_registry_cache_capacity);
    U(vrouter_xmlrpc_client_timeout_ms);
    U(vrouter_use_fencing);
    U(vrouter_send_sync_response);
    U(vrouter_min_workers);
    U(vrouter_max_workers);

#undef U

    local_node_()->update_config(pt, rep);
}

void
ObjectRouter::persist(bpt::ptree& pt,
                      const ReportDefault report_default) const
{
#define P(var)                                  \
    (var).persist(pt, report_default);

    P(vrouter_volume_read_threshold);
    P(vrouter_volume_write_threshold);
    P(vrouter_file_read_threshold);
    P(vrouter_file_write_threshold);
    P(vrouter_check_local_volume_potential_period);
    P(vrouter_backend_sync_timeout_ms);
    P(vrouter_migrate_timeout_ms);
    P(vrouter_redirect_timeout_ms);
    P(vrouter_redirect_retries);
    P(vrouter_routing_retries);
    P(vrouter_id);
    P(vrouter_cluster_id);
    P(vrouter_min_workers);
    P(vrouter_max_workers);
    P(vrouter_registry_cache_capacity);
    P(vrouter_xmlrpc_client_timeout_ms);
    P(vrouter_use_fencing);
    P(vrouter_send_sync_response);

#undef P

    local_node_()->persist_config(pt, report_default);
}

bool
ObjectRouter::checkConfig(const bpt::ptree& pt,
                          yt::ConfigurationReport& crep) const
{
    bool result = true;

    {
        ip::PARAMETER_TYPE(vrouter_min_workers) min(pt);
        ip::PARAMETER_TYPE(vrouter_min_workers) max(pt);

        if (min.value() == 0)
        {
            crep.push_front(yt::ConfigurationProblem(min.name(),
                                                     min.section_name(),
                                                     "Value must be > 0"));
            result = false;
        }

        if (max.value() < min.value())
        {
            crep.push_front(yt::ConfigurationProblem(max.name(),
                                                     max.section_name(),
                                                     "Value must be >= vrouter_min_workers"));

            result = false;
        }
    }

    {
        ip::PARAMETER_TYPE(vrouter_check_local_volume_potential_period) val(pt);
        if (val.value() == 0)
        {
            crep.push_front(yt::ConfigurationProblem(val.name(),
                                                     val.section_name(),
                                                     "Value must be > 0"));
            result = false;
        }
    }

    return result;
}

ObjectRouter::MaybeFailOverCacheConfig
ObjectRouter::failoverconfig_as_it_should_be() const
{
    switch (foc_config_mode_)
    {
    case FailOverCacheConfigMode::Manual:
        {
            LOCK_FOC_CONFIG();
            return foc_config_;
        }
    case FailOverCacheConfigMode::Automatic:
        {
            ASSERT(node_map_.size() >= 1);
            if (node_map_.size() == 1)
            {
                //running single node -> no failover configured
                return boost::none;
            }
            else
            {
                // All volumes on this node point to the failovercache on the
                // next node (cyclically).
                // The order of nodes is determined by their order in the map:
                // operator<(NodeId, NodeId) and will hence be the same on all
                // nodes if configured correctly.
                // The order of appearance of nodes in the json config file
                // should not matter.
                NodeId my_id = node_id();

                RLOCK_NODES();

                ConfigMap::const_iterator it = config_map_.find(my_id);
                ASSERT(it != config_map_.end());
                if(++it == config_map_.end())
                {
                    it = config_map_.begin();
                }

                return vd::FailOverCacheConfig(it->second.failovercache_host,
                                               it->second.failovercache_port,
                                               foc_mode_);
            }
        }
    }

    VERIFY(0 == "impossible code path entered");
}

void
ObjectRouter::mark_node_offline(const NodeId& vrouter_id)
{
    if (vrouter_id == node_id())
    {
        LOG_ERROR("Cannot set self offline " << vrouter_id);
        throw CannotSetSelfOfflineException("Cannot set myself offline");
    }
    cluster_registry_->set_node_state(vrouter_id, ClusterNodeStatus::State::Offline);
}

void
ObjectRouter::mark_node_online(const NodeId& node_id)
{
    cluster_registry_->set_node_state(node_id, ClusterNodeStatus::State::Online);
}

ClusterRegistry::NodeStatusMap
ObjectRouter::get_node_status_map()
{
    return cluster_registry_->get_node_status_map();
}

uint64_t
ObjectRouter::local_volume_potential(const boost::optional<vd::ClusterSize>& c,
                                     const boost::optional<vd::SCOMultiplier>& s,
                                     const boost::optional<vd::TLogMultiplier>& t)
{
    return local_node_()->volume_potential(c,
                                           s,
                                           t);
}

FailOverCacheConfigMode
ObjectRouter::get_foc_config_mode(const ObjectId& oid)
{
    return local_node_()->get_foc_config_mode(oid);
}

void
ObjectRouter::set_manual_foc_config(const ObjectId& oid,
                                    const MaybeFailOverCacheConfig& foc_config)
{
    local_node_()->set_manual_foc_config(oid, foc_config);
}

void
ObjectRouter::set_manual_default_foc_config(const MaybeFailOverCacheConfig& foc_config)
{
    local_node_()->set_manual_default_foc_config(foc_config);
    LOCK_FOC_CONFIG();
    foc_config_ = foc_config;
}

void
ObjectRouter::set_automatic_foc_config(const ObjectId& oid)
{
    local_node_()->set_automatic_foc_config(oid);
}

std::unique_ptr<PythonClient>
ObjectRouter::xmlrpc_client()
{
    std::vector<ClusterContact> contacts;

    {
        RLOCK_NODES();

        contacts.reserve(config_map_.size());

        auto it = config_map_.find(node_id());
        VERIFY(it != config_map_.end());

        contacts.emplace_back(it->second.xmlrpc_host,
                              it->second.xmlrpc_port);

        for (const auto& p : config_map_)
        {
            if (p.first != node_id())
            {
                contacts.emplace_back(p.second.xmlrpc_host,
                                      p.second.xmlrpc_port);
            }
        }
    }

    return std::make_unique<PythonClient>(cluster_id(),
                                          contacts,
                                          boost::chrono::seconds(vrouter_xmlrpc_client_timeout_ms.value() / 1000));
}

bool
ObjectRouter::fencing_support_() const
{
    return api::fencing_support() and vrouter_use_fencing.value();
}

volumedriver::DtlInSync
ObjectRouter::dtl_in_sync_(const ObjectId& oid) const
{
    vd::DtlInSync dtl_in_sync = vd::DtlInSync::F;
    bool found = false;

    {
        LOCK_REDIRECTS();

        auto it = redirects_.find(oid);
        if (it != redirects_.end())
        {
            found = true;
            dtl_in_sync = it->second.dtl_in_sync;
        }
    }

    // VERIFY(found) instead?
    if (found)
    {
        LOG_INFO(oid << ": DtlInSync = " << dtl_in_sync);

    }
    else
    {
        LOG_WARN(oid << " not found in redirects map, assuming DtlInSync = " << vd::DtlInSync::F);
    }

    return dtl_in_sync;
}

void
ObjectRouter::set_dtl_in_sync(const ObjectId& oid,
                              const vd::DtlInSync dtl_in_sync)
{
    ObjectRegistrationPtr reg(object_registry_->find_throw(oid,
                                                           IgnoreCache::F));
    if (reg->node_id != local_node_()->node_id())
    {
        LOCK_REDIRECTS();
        redirects_[oid].dtl_in_sync = dtl_in_sync;
    }
}

const ScrubManager&
ObjectRouter::scrub_manager() const
{
    return local_node_()->scrub_manager();
}

}
