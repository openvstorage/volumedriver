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

#include "FileSystem.h"
#include "FileSystemEvents.h"
#include "LocalNode.h"
#include "MessageUtils.h"
#include "Messages.pb.h"
#include "Protocol.h"
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
                           const boost::optional<vd::FailOverCacheConfig>& foc_config,
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
    , larakoon_(larakoon)
    , object_registry_(std::make_shared<CachedObjectRegistry>(cluster_id(),
                                                              node_id(),
                                                              larakoon_,
                                                              vrouter_registry_cache_capacity.value()))
    , ztx_(new zmq::context_t(::sysconf(_SC_NPROCESSORS_ONLN)))
    , publisher_(std::make_shared<EventPublisher>(cluster_id(),
                                                  node_id(),
                                                  pt))
    , foc_config_mode_(foc_config_mode)
    , foc_mode_(foc_mode)
    , foc_config_(foc_config)
{
    LOG_TRACE("setting up");

    THROW_WHEN(vrouter_check_local_volume_potential_period.value() == 0);

    cluster_registry_ = std::make_shared<ClusterRegistry>(cluster_id(),
                                                          larakoon_);
    VERIFY(cluster_registry_);
    update_node_map_(pt);

    const ClusterNodeConfig ncfg(node_config());
    const std::string addr("tcp://" +
                           ncfg.host +
                           ":"s +
                           boost::lexical_cast<std::string>(ncfg.message_port));

    worker_pool_.reset(new ZWorkerPool("ObjectRouterWorkerPool",
                                       *ztx_,
                                       addr,
                                       [this](ZWorkerPool::MessageParts&& parts)
                                       {
                                           return redirected_work_(std::move(parts));
                                       },
                                       vrouter_min_workers.value(),
                                       vrouter_max_workers.value()));
}

ObjectRouter::~ObjectRouter()
{
    // ZMQ teardown
    // (1) destruct (close) all req sockets and all shared_ptrs to the ztx_
    node_map_.clear();

    // (2) this will close the ZMQ context and lead to ZWorkerPool shutting down
    //     synchronously.
    ztx_ = nullptr;

    LOG_INFO("Bye");
}

ObjectRouter::NodeMap
ObjectRouter::build_node_map_(const boost::optional<const bpt::ptree&>& pt)
{
    const ClusterRegistry::NodeStatusMap status_map(get_node_status_map());
    NodeMap new_map;

    for (const auto& v : status_map)
    {
        const ClusterNodeConfig& cfg = v.second.config;
        std::shared_ptr<ClusterNode> old(find_node_(cfg.vrouter_id));
        if (old)
        {
            if (old->config != cfg)
            {
                LOG_ERROR("Refusing to modify an existing ClusterNodeConfig. Old: " <<
                          old->config << ", new: " << cfg);
                throw InvalidConfigurationException("Modifying an existing node config is not supported",
                                                    cfg.vrouter_id.str().c_str());
            }
        }

        std::shared_ptr<ClusterNode> n;

        if (cfg.vrouter_id == node_id())
        {
            if (pt)
            {
                n = std::shared_ptr<ClusterNode>(new LocalNode(*this,
                                                               cfg,
                                                               *pt));

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
                VERIFY(old);
                n = old;
            }
        }
        else
        {
            n = std::shared_ptr<ClusterNode>(new RemoteNode(*this,
                                                            cfg,
                                                            ztx_));
        }

        const auto res(new_map.emplace(std::make_pair(cfg.vrouter_id,
                                                      n)));
        VERIFY(res.second);
    }

    auto it = new_map.find(node_id());
    if (it == new_map.end())
    {
        LOG_ERROR("Our very own node ID " << node_id() <<
                  " does not appear in the cluster nodes config");
        throw InvalidConfigurationException("Missing node ID in cluster config",
                                            node_id().str().c_str());
    }

    return new_map;
}

void
ObjectRouter::update_node_map_(const boost::optional<const bpt::ptree&>& pt)
{
    NodeMap new_map(build_node_map_(pt));

    LOG_INFO("New ClusterNodeConfigs:");

    for (const auto& p : new_map)
    {
        LOG_INFO("\t" << p.second->config);
    }

    WLOCK_NODES();

    std::swap(node_map_,
              new_map);
}

void
ObjectRouter::update_cluster_node_configs()
{
    update_node_map_(boost::none);
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

    auto it = node_map_.find(node_id);
    if (it == node_map_.end())
    {
        return boost::none;
    }
    else
    {
        return it->second->config;
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
ObjectRouter::redirected_work_(ZWorkerPool::MessageParts&& parts_in)
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
                handle_sync_(get_req<vfsprotocol::SyncRequest>(parts_in));
                break;
            }
        case vfsprotocol::RequestType::GetSize:
            {
                CHECK(parts_in.size() == 3);
                parts_out.emplace_back(handle_get_size_(get_req<vfsprotocol::GetSizeRequest>(parts_in)));
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

template<typename R,
         typename... A>
R
ObjectRouter::maybe_steal_(R (ClusterNode::*fn)(const Object&,
                                                A... args),
                           IsRemoteNode& remote,
                           AttemptTheft attempt_theft,
                           const ObjectRegistration& reg,
                           A... args)
{
    const NodeId& owner_id = reg.node_id;
    const ObjectId& id = reg.volume_id;

    LOG_TRACE(id << ", owner " << owner_id);

    unsigned retry = 0;

    while (true)
    {
        std::shared_ptr<ClusterNode> node(find_node_or_throw_(owner_id));
        remote = std::dynamic_pointer_cast<RemoteNode>(node) != nullptr ?
            IsRemoteNode::T :
            IsRemoteNode::F;

        try
        {
            ClusterNode& cn = *node;
            return (cn.*fn)(reg.object(), std::forward<A>(args)...);
        }
        catch (RequestTimeoutException&)
        {
            VERIFY(remote == IsRemoteNode::T);
            LOG_ERROR(id << ": remote node " << owner_id << " timed out");

            if (cluster_registry_->get_node_state(owner_id) ==
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

            const bool stolen = steal_(reg,
                                       OnlyStealFromOfflineNode::T);
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
                     OnlyStealFromOfflineNode only_steal_if_offline)
{
    LOG_INFO("Checking whether we should steal " << reg.volume_id << " from " <<
             reg.node_id << ", only steal if offline: " << only_steal_if_offline);
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
    }
    catch (ClusterNodeNotOfflineException&)
    {
        return false;
    }

    try
    {
        backend_restart_(reg.object(),
                         ForceRestart::T,
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

template<typename R,
         typename... A>
R
ObjectRouter::do_route_(R (ClusterNode::*fn)(const Object&,
                                             A... args),
                        IsRemoteNode& remote,
                        AttemptTheft attempt_theft,
                        const ObjectId& id,
                        A... args)
{
    LOG_TRACE(id);

    ObjectRegistrationPtr reg(object_registry_->find_throw(id,
                                                           IgnoreCache::F));

    return do_route_(fn,
                     remote,
                     attempt_theft,
                     reg,
                     std::forward<A>(args)...);
}

template<typename R, typename... A>
R
ObjectRouter::do_route_(R (ClusterNode::*fn)(const Object&,
                                             A... args),
                        IsRemoteNode& remote,
                        AttemptTheft attempt_theft,
                        ObjectRegistrationPtr reg,
                        A... args)
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
                                std::forward<A>(args)...);
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

template<typename R, typename... A>
R
ObjectRouter::route_(R (ClusterNode::*fn)(const Object&,
                                          A... args),
                     AttemptTheft attempt_theft,
                     const ObjectId& id,
                     A... args)
{
    IsRemoteNode remote = IsRemoteNode::F;
    return do_route_(fn, remote, attempt_theft, id, args...);
}

// TODO: It'd be nice to make this work with arbitrary return types.
template<typename P,
         typename... A>
void
ObjectRouter::maybe_migrate_(P&& migrate_pred,
                             void (ClusterNode::*fn)(const Object&,
                                                     A... args),
                             const ObjectId& id,
                             A... args)
{
    ObjectRegistrationPtr reg(object_registry_->find_throw(id,
                                                           IgnoreCache::F));

    IsRemoteNode remote = IsRemoteNode::F;
    do_route_(fn,
              remote,
              AttemptTheft::T,
              reg,
              std::forward<A>(args)...);

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
                migrate_(*reg,
                         OnlyStealFromOfflineNode::T,
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

void
ObjectRouter::write(const ObjectId& id,
                    const uint8_t* buf,
                    size_t size,
                    off_t off)
{
    LOG_TRACE(id << ": size " << size << ", off " << off);

    auto pred([this](const ObjectId& id,
                     const ObjectType tp)
              {
                  LOCK_REDIRECTS();

                  return migrate_pred_helper_("write",
                                              id,
                                              tp != ObjectType::File,
                                              tp == ObjectType::File ?
                                              vrouter_file_write_threshold.value() :
                                              vrouter_volume_write_threshold.value(),
                                              redirects_[id].writes);
              });

    maybe_migrate_(std::move(pred),
                   &ClusterNode::write,
                   id,
                   buf,
                   &size,
                   off);
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

    size_t size = req.size();
    local_node_()->write(obj,
                         reinterpret_cast<const uint8_t*>(data.data()),
                         &size,
                         req.offset());

    const auto rsp(vfsprotocol::MessageUtils::create_write_response(size));
    return ZUtils::serialize_to_message(rsp);
}

void
ObjectRouter::read(const ObjectId& id,
                   uint8_t* buf,
                   size_t& size,
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

    maybe_migrate_(std::move(pred),
                   &ClusterNode::read,
                   id,
                   buf,
                   &size,
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

void
ObjectRouter::sync(const ObjectId& id)
{
    LOG_TRACE(id);

    route_(&ClusterNode::sync,
           AttemptTheft::T,
           id);
}

void
ObjectRouter::handle_sync_(const vfsprotocol::SyncRequest& req)
{
    const Object obj(obj_from_msg(req));
    LOG_TRACE(obj);
    local_node_()->sync(obj);
}

uint64_t
ObjectRouter::get_size(const ObjectId& id)
{
    LOG_TRACE(id);

    return route_(&ClusterNode::get_size,
                  AttemptTheft::T,
                  id);
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

void
ObjectRouter::resize(const ObjectId& id, uint64_t newsize)
{
    LOG_TRACE(id << ", newsize " << newsize);

    route_(&ClusterNode::resize,
           AttemptTheft::T,
           id,
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

    try
    {
        route_(&ClusterNode::unlink,
               AttemptTheft::F,
               id);
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
                           only_steal_if_offline))
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
                        std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config,
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
             OnlyStealFromOfflineNode::F,
             force);
}

void
ObjectRouter::stop(const ObjectId& id,
                   vd::DeleteLocalData delete_local_data)
{
    LOG_INFO("Attempting to stop " << id);

    ObjectRegistrationPtr reg(object_registry_->find_throw(id,
                                                           IgnoreCache::T));

    if (reg->node_id != node_id())
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
ObjectRouter::set_volume_as_template(const vd::VolumeId& id)
{
    LOG_INFO("Set volume " << id << " as template");
    TODO("AR: revisit conversion to ObjectId at this point");
    local_node_()->set_volume_as_template(ObjectId(id.str()));
}

void
ObjectRouter::rollback_volume(const ObjectId& volume_id,
                              const vd::SnapshotName& snap_id)
{
    LOG_INFO("Rolling back " << volume_id << " to snapshot " << snap_id);

    local_node_()->rollback_volume(volume_id,
                                   snap_id);
}

void
ObjectRouter::delete_snapshot(const ObjectId& oid,
                              const vd::SnapshotName& snap)
{
    LOG_INFO("Deleting snapshot " << snap << " from " << oid);
    local_node_()->delete_snapshot(oid,
                                   snap);
}

void
ObjectRouter::get_scrub_work(const ObjectId& oid,
                             const boost::optional<vd::SnapshotName>& start_snap,
                             const boost::optional<vd::SnapshotName>& end_snap,
                             std::vector<std::string>& work)
{
    LOG_INFO(oid << ": getting scrub work, start snapshot " << start_snap <<
             ", end snapshot " << end_snap);
    local_node_()->get_scrub_work(oid,
                                  start_snap,
                                  end_snap,
                                  work);
}

void
ObjectRouter::apply_scrub_result(const ObjectId& oid,
                                 const std::string& result)
{
    LOG_INFO(oid << ": applying scrub result");
    local_node_()->apply_scrub_result(oid,
                                      result);
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

    ip::PARAMETER_TYPE(vrouter_min_workers) min(pt);
    ip::PARAMETER_TYPE(vrouter_min_workers) max(pt);

    try
    {
        if (min.value() != vrouter_min_workers.value() or
            max.value() != vrouter_max_workers.value())
        {
            worker_pool_->resize(min.value(),
                                 max.value());
        }

        U(vrouter_min_workers);
        U(vrouter_max_workers);
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Failed to resize worker pool: " << EWHAT);
            rep.no_update(min.name(),
                          vrouter_min_workers.value(),
                          min.value());
            rep.no_update(max.name(),
                          vrouter_max_workers.value(),
                          max.value());
        });

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

boost::optional<vd::FailOverCacheConfig>
ObjectRouter::failoverconfig_as_it_should_be() const
{
    switch (foc_config_mode_)
    {
    case FailOverCacheConfigMode::Manual:
        {
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
                NodeMap::const_iterator it = node_map_.find(my_id);
                ASSERT(it != node_map_.end());
                if(++it == node_map_.end())
                {
                    it = node_map_.begin();
                }

                return vd::FailOverCacheConfig(it->second->config.host,
                                               it->second->config.failovercache_port,
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
ObjectRouter::local_volume_potential(const boost::optional<vd::SCOMultiplier>& s,
                                     const boost::optional<vd::TLogMultiplier>& t)
{
    return local_node_()->volume_potential(s,
                                           t);
}

FailOverCacheConfigMode
ObjectRouter::get_foc_config_mode(const ObjectId& oid)
{
    return local_node_()->get_foc_config_mode(oid);
}

void
ObjectRouter::set_manual_foc_config(const ObjectId& oid,
                                    const boost::optional<volumedriver::FailOverCacheConfig>& foc_config)
{
    local_node_()->set_manual_foc_config(oid, foc_config);
}

void
ObjectRouter::set_automatic_foc_config(const ObjectId& oid)
{
    local_node_()->set_automatic_foc_config(oid);
}

}
