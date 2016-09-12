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

#ifndef VFS_OBJECT_ROUTER_H_
#define VFS_OBJECT_ROUTER_H_

#include "CachedObjectRegistry.h"
#include "CloneFileFlags.h"
#include "ClusterRegistry.h"
#include "FailOverCacheConfigMode.h"
#include "FileSystemParameters.h"
#include "ForceRestart.h"
#include "LocalNode.h"
#include "Object.h"
#include "ZWorkerPool.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/optional.hpp>

#include <cppzmq/zmq.hpp>

#include <youtils/BooleanEnum.h>
#include <youtils/InitializedParam.h>
#include <youtils/Logging.h>
#include <youtils/VolumeDriverComponent.h>

#include <volumedriver/Api.h>
#include <volumedriver/Events.h>
#include <volumedriver/VolumeDriverParameters.h>

// Fwd declarations to avoid having to include the generated Messages.pb.h
// to make life for downstream consumers (ganesha plugin) of this header easier.
namespace vfsprotocol
{

class PingMessage;
class ReadRequest;
class WriteRequest;
class SyncRequest;
class GetSizeRequest;
class ResizeRequest;
class DeleteRequest;
class TransferRequest;

}

namespace scrubbing
{
class ScrubReply;
class ScrubWork;
}

namespace volumedriver
{
class Volume;
}

namespace volumedriverfstest
{
class FileSystemTestBase;
class ObjectRouterTest;
}

namespace volumedriverfs
{

class ClusterNode;
class FileSystem;
class LocalNode;
class LockedArakoon;
class PythonClient;
class RemoteNode;

VD_BOOLEAN_ENUM(OnlyStealFromOfflineNode);
VD_BOOLEAN_ENUM(IsRemoteNode);
VD_BOOLEAN_ENUM(AttemptTheft);

MAKE_EXCEPTION(Exception, fungi::IOException);
MAKE_EXCEPTION(RemoteException, Exception);
MAKE_EXCEPTION(RequestTimeoutException, Exception);
MAKE_EXCEPTION(RemoteTimeoutException, Exception);
MAKE_EXCEPTION(ObjectNotRunningHereException, Exception);
MAKE_EXCEPTION(ClusterNodeNotOnlineException, Exception);
MAKE_EXCEPTION(ClusterNodeNotOfflineException, Exception);
MAKE_EXCEPTION(CannotSetSelfOfflineException, Exception);
MAKE_EXCEPTION(ObjectStillHasChildrenException, Exception);

// Distributed volume access - the idea is as follows:
// (1) ClusterNode running the volume is looked up in the registry
// (2) Action is invoked on ClusterNode. If it's a LocalNode the action is carried
//     out right away, in case it's a RemoteNode a request is sent to the remote
//     ObjectRouter which will invoke the action on its LocalNode and send a response
//     back.
//
// Volume migration is the act of sending a Transfer message to a remote node and
// restarting the volume on the local node. It can be  kicked off either
// * externally (through a management call)
// * internally, if the read/write threshold is exceeded
// .
//
// ZMQ details:
// The ROUTER socket is driven by its own thread. It reads requests and executes them
// (after some basic sanity checking, which probably can do with some improvements).
// The REQ socket (in the RemoteNode) is used to send a message to the remote ROUTER
// socket. This typically happens from a FUSE thread with the exception of volume
// migration.
//
// TODO:
// * The message passing is *synchronous* (due to ZMQ REQ/ROUTER)
// * The frequent lookups (volume id -> node id, volume id -> volume ptr in LocalNode)
//   will hurt local performance.
// * Experiment with a ZMQ inproc socket for the local ClusterNode / ObjectRouter
//   communication (ad-hoc tests with the inproc_lat that comes with ZMQ suggest 16us per
//   message for 100k roundtrips with 1k ... 64k messages). But that also needs
//   a different requester socket type / owner (ROUTER sock per fuse thread?)
//   .

class ObjectRouter
    : public youtils::VolumeDriverComponent
{
    friend class volumedriverfstest::FileSystemTestBase;
    friend class volumedriverfstest::ObjectRouterTest;

public:
    using MaybeFailOverCacheConfig = boost::optional<volumedriver::FailOverCacheConfig>;

    ObjectRouter(const boost::property_tree::ptree& pt,
                 std::shared_ptr<youtils::LockedArakoon> larakoon,
                 const FailOverCacheConfigMode,
                 const volumedriver::FailOverCacheMode,
                 const MaybeFailOverCacheConfig&,
                 const RegisterComponent registerize = RegisterComponent::T);

    ~ObjectRouter();

    ObjectRouter(const ObjectRouter&) = delete;

    ObjectRouter&
    operator=(const ObjectRouter&) = delete;

    static void
    destroy(std::shared_ptr<youtils::LockedArakoon> larakoon,
            const boost::property_tree::ptree& pt);

    void
    update_cluster_node_configs();

    void
    ping(const NodeId& id);

    FastPathCookie
    write(const FastPathCookie&,
          const ObjectId&,
          const uint8_t* buf,
          size_t size,
          off_t off);

    FastPathCookie
    read(const FastPathCookie&,
         const ObjectId&,
         uint8_t* buf,
         size_t& size,
         off_t off);

    FastPathCookie
    sync(const FastPathCookie&,
         const ObjectId&);

    uint64_t
    get_size(const ObjectId& id);

    void
    resize(const ObjectId& id,
           uint64_t newsize);

    void
    unlink(const ObjectId& id);

    void
    create(const Object& obj,
           std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config);

    void
    create_clone(const volumedriver::VolumeId& clone_id,
                 std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config,
                 const volumedriver::VolumeId& parent_id,
                 const MaybeSnapshotName& maybe_parent_snap);

    void
    clone_to_existing_volume(const volumedriver::VolumeId& clone_id,
                             std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config,
                             const volumedriver::VolumeId& parent_id,
                             const MaybeSnapshotName& maybe_parent_snap);

    void
    vaai_copy(const ObjectId& src_id,
              const boost::optional<ObjectId>& maybe_dst_id,
              std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config,
              const uint64_t timeout,
              const CloneFileFlags flags,
              VAAICreateCloneFun fun);

    void
    vaai_filecopy(const ObjectId& src_id,
                  const ObjectId& dst_id);

    void
    migrate(const ObjectId& id,
            ForceRestart = ForceRestart::F);

    bool
    maybe_restart(const ObjectId& id,
                  ForceRestart = ForceRestart::F);

    void
    restart(const ObjectId& id,
            ForceRestart force = ForceRestart::F);

    void
    stop(const ObjectId& id,
         volumedriver::DeleteLocalData = volumedriver::DeleteLocalData::T);

    void
    set_volume_as_template_local(const volumedriver::VolumeId& id);

    void
    create_snapshot_local(const ObjectId& volume_id,
                          const volumedriver::SnapshotName& snap_id,
                          const int64_t timeout);

    void
    create_snapshot(const ObjectId& volume_id,
                    const volumedriver::SnapshotName& snap_id,
                    const int64_t timeout);

    void
    rollback_volume_local(const ObjectId&,
                          const volumedriver::SnapshotName&);

    void
    rollback_volume(const ObjectId&,
                    const volumedriver::SnapshotName&);

    void
    delete_snapshot_local(const ObjectId&,
                          const volumedriver::SnapshotName&);

    void
    delete_snapshot(const ObjectId&,
                    const volumedriver::SnapshotName&);

    std::list<volumedriver::SnapshotName>
    list_snapshots_local(const ObjectId&);

    std::vector<std::string>
    list_snapshots(const ObjectId&);

    bool
    is_volume_synced_up_to_local(const ObjectId&,
                                 const volumedriver::SnapshotName&);

    bool
    is_volume_synced_up_to(const ObjectId&,
                           const volumedriver::SnapshotName&);

    std::vector<scrubbing::ScrubWork>
    get_scrub_work_local(const ObjectId& oid,
                         const boost::optional<volumedriver::SnapshotName>& start_snap,
                         const boost::optional<volumedriver::SnapshotName>& end_snap);

    void
    queue_scrub_reply_local(const ObjectId& oid,
                            const scrubbing::ScrubReply&);

    void
    mark_node_offline(const NodeId& node_id);

    void
    mark_node_online(const NodeId& node_id);

    ClusterRegistry::NodeStatusMap
    get_node_status_map();

    virtual const char*
    componentName() const override final;

    virtual void
    update(const boost::property_tree::ptree& pt,
           youtils::UpdateReport& rep) override final;

    virtual void
    persist(boost::property_tree::ptree& pt,
            const ReportDefault reportDefault) const override final;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                youtils::ConfigurationReport&) const override final;

    std::shared_ptr<CachedObjectRegistry>
    object_registry()
    {
        return object_registry_;
    }

    std::shared_ptr<ClusterRegistry>
    cluster_registry()
    {
        return cluster_registry_;
    }

    ClusterId
    cluster_id() const
    {
        return ClusterId(vrouter_cluster_id.value());
    }

    NodeId
    node_id() const
    {
        return NodeId(vrouter_id.value());
    }

    boost::optional<ClusterNodeConfig>
    node_config(const NodeId&) const;

    ClusterNodeConfig
    node_config() const;

    boost::chrono::milliseconds
    redirect_timeout() const
    {
        return boost::chrono::milliseconds(vrouter_redirect_timeout_ms.value());
    }

    boost::chrono::milliseconds
    backend_sync_timeout() const
    {
        return boost::chrono::milliseconds(vrouter_backend_sync_timeout_ms.value());
    }

    boost::chrono::milliseconds
    migrate_timeout() const
    {
        return boost::chrono::milliseconds(vrouter_migrate_timeout_ms.value());
    }

    MaybeFailOverCacheConfig
    failoverconfig_as_it_should_be() const;

    std::shared_ptr<events::PublisherInterface>
    event_publisher() const
    {
        return publisher_;
    }

    uint64_t
    local_volume_potential(const boost::optional<volumedriver::ClusterSize>&,
                           const boost::optional<volumedriver::SCOMultiplier>&,
                           const boost::optional<volumedriver::TLogMultiplier>&);

    FailOverCacheConfigMode
    get_foc_config_mode(const ObjectId&);

    FailOverCacheConfigMode
    get_default_foc_config_mode() const
    {
        return foc_config_mode_;
    }

    void
    set_manual_default_foc_config(const MaybeFailOverCacheConfig&);

    void
    set_manual_foc_config(const ObjectId&,
                          const MaybeFailOverCacheConfig&);

    void
    set_automatic_foc_config(const ObjectId&);

    std::unique_ptr<PythonClient>
    xmlrpc_client();

private:
    DECLARE_LOGGER("VFSObjectRouter");

    mutable fungi::RWLock node_map_lock_;

    using NodeMap = std::unordered_map<NodeId, std::shared_ptr<ClusterNode>>;
    NodeMap node_map_;

    DECLARE_PARAMETER(vrouter_volume_read_threshold);
    DECLARE_PARAMETER(vrouter_volume_write_threshold);
    DECLARE_PARAMETER(vrouter_file_read_threshold);
    DECLARE_PARAMETER(vrouter_file_write_threshold);
    DECLARE_PARAMETER(vrouter_check_local_volume_potential_period);
    DECLARE_PARAMETER(vrouter_backend_sync_timeout_ms);
    DECLARE_PARAMETER(vrouter_migrate_timeout_ms);
    DECLARE_PARAMETER(vrouter_redirect_timeout_ms);
    DECLARE_PARAMETER(vrouter_redirect_retries);
    DECLARE_PARAMETER(vrouter_routing_retries);
    DECLARE_PARAMETER(vrouter_id);
    DECLARE_PARAMETER(vrouter_cluster_id);
    DECLARE_PARAMETER(vrouter_min_workers);
    DECLARE_PARAMETER(vrouter_max_workers);
    DECLARE_PARAMETER(vrouter_registry_cache_capacity);
    DECLARE_PARAMETER(vrouter_xmlrpc_client_timeout_ms);
    DECLARE_PARAMETER(vrouter_use_fencing);

    std::shared_ptr<youtils::LockedArakoon> larakoon_;
    std::shared_ptr<CachedObjectRegistry> object_registry_;
    std::shared_ptr<ClusterRegistry> cluster_registry_;
    std::shared_ptr<zmq::context_t> ztx_;
    std::shared_ptr<events::PublisherInterface> publisher_;

    std::unique_ptr<ZWorkerPool> worker_pool_;

    FailOverCacheConfigMode foc_config_mode_;
    volumedriver::FailOverCacheMode foc_mode_;
    mutable std::mutex foc_config_lock_;
    MaybeFailOverCacheConfig foc_config_;

    struct RedirectCounter
    {
        RedirectCounter()
            : reads(0)
            , writes(0)
        {}

        uint64_t reads;
        uint64_t writes;
    };

    std::map<ObjectId, RedirectCounter> redirects_;

    mutable std::mutex redirects_lock_;

    void
    update_node_map_(const boost::optional<const boost::property_tree::ptree&>& pt);

    NodeMap
    build_node_map_(const boost::optional<const boost::property_tree::ptree&>& pt);

    ZWorkerPool::MessageParts
    redirected_work_(ZWorkerPool::MessageParts&& parts_in);

    std::shared_ptr<ClusterNode>
    find_node_(const NodeId&) const;

    std::shared_ptr<ClusterNode>
    find_node_or_throw_(const NodeId&) const;

    std::shared_ptr<LocalNode>
    local_node_() const;

    template<typename Ret,
             typename... Args>
    Ret
    maybe_steal_(Ret (ClusterNode::*fn)(const Object&,
                                        Args...),
                 IsRemoteNode&,
                 AttemptTheft,
                 const ObjectRegistration&,
                 FastPathCookie&,
                 Args...);

    template<typename Ret,
             typename... Args>
    Ret
    do_route_(Ret (ClusterNode::*fn)(const Object&,
                                     Args...),
              IsRemoteNode&,
              AttemptTheft,
              const ObjectId&,
              FastPathCookie&,
              Args...);

    template<typename Ret,
             typename... Args>
    Ret
    do_route_(Ret (ClusterNode::*fn)(const Object&,
                                     Args...),
              IsRemoteNode&,
              AttemptTheft,
              ObjectRegistrationPtr,
              FastPathCookie&,
              Args...);

    template<typename Ret,
             typename... Args>
    Ret
    route_(Ret (ClusterNode::*fn)(const Object&,
                                  Args...),
           AttemptTheft,
           const ObjectId&,
           FastPathCookie&,
           Args...);

    bool
    migrate_pred_helper_(const char* desc,
                         const ObjectId& id,
                         bool is_volume,
                         uint64_t thresh,
                         uint64_t& counter) const;

    // migrate_pred needs to have the following signature:
    // bool(const ObjectId&, ObjectType).
    // We're not using a std::function here as that ends up allocating memory
    // which we want to avoid. We could pass function pointers / references but that
    // limits its flexibility.
    template<typename MigratePred,
             typename... Args>
    FastPathCookie
    maybe_migrate_(MigratePred&& ,
                   void (ClusterNode::*fn)(const Object&,
                                           Args...),
                   const ObjectId&,
                   Args...);

    void
    handle_message_(zmq::socket_t& router_sock);

    zmq::message_t
    handle_ping_(const vfsprotocol::PingMessage& req);

    zmq::message_t
    handle_read_(const vfsprotocol::ReadRequest& req);

    zmq::message_t
    handle_write_(const vfsprotocol::WriteRequest& req,
                  const zmq::message_t& data);


    void
    handle_sync_(const vfsprotocol::SyncRequest& req);

    zmq::message_t
    handle_get_size_(const vfsprotocol::GetSizeRequest& req);

    void
    handle_resize_(const vfsprotocol::ResizeRequest& req);

    void
    handle_delete_volume_(const vfsprotocol::DeleteRequest& req);

    void
    handle_transfer_(const vfsprotocol::TransferRequest& req);

    void
    migrate_(const ObjectRegistration& reg,
             OnlyStealFromOfflineNode only_steal_if_offline,
             ForceRestart force);

    bool
    steal_(const ObjectRegistration& reg,
           OnlyStealFromOfflineNode only_steal_if_offline);

    void
    maybe_register_base_(const ObjectId& id,
                         const char* desc,
                         std::function<void(const ObjectId&,
                                            const backend::Namespace&)>&& fn);

    using PrepareRestartFun = std::function<void(const Object&)>;

    void
    backend_restart_(const Object& obj,
                     ForceRestart force,
                     PrepareRestartFun prep_restart_fun);

    FastPathCookie
    write_(const ObjectId&,
           const uint8_t* buf,
           size_t* size,
           off_t off);

    FastPathCookie
    read_(const ObjectId&,
          uint8_t* buf,
          size_t* size,
          off_t off);

    FastPathCookie
    sync_(const ObjectId&);

    template<typename... Args>
    FastPathCookie
    select_path_(const FastPathCookie&,
                 FastPathCookie (ObjectRouter::*slow_fun)(Args...),
                 FastPathCookie (LocalNode::*fast_fun)(const FastPathCookie&,
                                                       Args...),
                 Args...);

    bool
    fencing_support_() const;
};

}

#endif // VFS_OBJECT_ROUTER_H_
