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

#ifndef VFS_LOCAL_NODE_H_
#define VFS_LOCAL_NODE_H_

#include "CloneFileFlags.h"
#include "ClusterNode.h"
#include "ClusterNodeConfig.h"
#include "FastPathCookie.h"
#include "FileSystemParameters.h"
#include "ForceRestart.h"
#include "NodeId.h"
#include "Object.h"

#include <boost/property_tree/ptree_fwd.hpp>

#include <youtils/ArakoonInterface.h>
#include <youtils/Logging.h>
#include <youtils/PeriodicAction.h>

#include <volumedriver/Api.h>
#include <volumedriver/ClusterLocation.h>
#include <volumedriver/FailOverCacheConfig.h>
#include <volumedriver/TransientException.h>
#include <volumedriver/VolumeDriverParameters.h>

#include <filedriver/ContainerManager.h>

namespace scrubbing
{
class ScrubReply;
class ScrubWork;
}

namespace volumedriverfstest
{
class LocalNodeTest;
}

namespace volumedriverfs
{

class ObjectRegistration;
class ObjectRegistry;
class ObjectRouter;
class ScrubManager;

MAKE_EXCEPTION(OwnershipException, fungi::IOException);
MAKE_EXCEPTION(TimeoutException, fungi::IOException);
MAKE_EXCEPTION(SyncTimeoutException, TimeoutException);

typedef std::function<void(const MaybeSnapshotName&,
                           std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config)> VAAICreateCloneFun;

class LocalNode final
    : public ClusterNode
{
    friend class volumedriverfstest::LocalNodeTest;

public:
    LocalNode(ObjectRouter& vrouter,
              const ClusterNodeConfig& cfg,
              const boost::property_tree::ptree& pt);

    virtual ~LocalNode();

    LocalNode(const LocalNode&) = delete;

    LocalNode&
    operator=(const LocalNode&) = delete;

    static void
    destroy(ObjectRegistry& reg,
            const boost::property_tree::ptree& pt);

    virtual void
    read(const Object& id,
         uint8_t* buf,
         size_t* size,
         const off_t off) override final;

    virtual void
    write(const Object& obj,
          const uint8_t* buf,
          size_t* size,
          const off_t off,
          volumedriver::DtlInSync&) override final;

    virtual void
    sync(const Object&,
         volumedriver::DtlInSync&) override final;

    virtual uint64_t
    get_size(const Object& obj) override final;

    virtual volumedriver::ClusterMultiplier
    get_cluster_multiplier(const Object& obj) override final;

    virtual volumedriver::CloneNamespaceMap
    get_clone_namespace_map(const Object& obj) override final;

    virtual std::vector<volumedriver::ClusterLocation>
    get_page(const Object& obj,
             const volumedriver::ClusterAddress ca) override final;

    virtual void
    resize(const Object& obj,
           uint64_t newsize) override final;

    virtual void
    unlink(const Object& obj) override final;

    void
    stop(const Object& obj,
         volumedriver::DeleteLocalData = volumedriver::DeleteLocalData::T);

    void
    create(const Object& obj,
           std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config);

    void
    create_clone(const ObjectId& clone_id,
                 std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config,
                 const ObjectId& parent_id,
                 const MaybeSnapshotName& maybe_parent_snap);

    void
    clone_to_existing_volume(const ObjectId& clone_id,
                             std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config,
                             const ObjectId& parent_id,
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
    remove_local_data(const Object& obj);

    void
    local_restart(const ObjectRegistration&,
                  ForceRestart force);

    using PrepareRestartFun = std::function<void(const Object&)>;

    // `prep_restart_fun' is run with exclusive access to the volume.
    void
    backend_restart(const Object& obj,
                    ForceRestart force,
                    PrepareRestartFun prep_restart_fun);

    using MaybeSyncTimeoutMilliSeconds = boost::optional<boost::chrono::milliseconds>;
    using Clock = boost::chrono::steady_clock;
    using Deadline = Clock::time_point;

    void
    transfer(const Object& obj,
             const NodeId target_node,
             MaybeSyncTimeoutMilliSeconds);

    void
    create_snapshot(const ObjectId& id,
                    const volumedriver::SnapshotName& snap_id,
                    const int64_t timeout);

    void
    rollback_volume(const ObjectId& volume_id,
                    const volumedriver::SnapshotName& snap_id);

    void
    delete_snapshot(const ObjectId& volume_id,
                    const volumedriver::SnapshotName& snap_id);

    std::list<volumedriver::SnapshotName>
    list_snapshots(const ObjectId& id);

    bool
    is_volume_synced_up_to(const ObjectId& id,
                           const volumedriver::SnapshotName& snap_id);

    std::vector<scrubbing::ScrubWork>
    get_scrub_work(const ObjectId& oid,
                   const boost::optional<volumedriver::SnapshotName>& start_snap,
                   const boost::optional<volumedriver::SnapshotName>& end_snap);

    void
    set_volume_as_template(const ObjectId& id);

    uint64_t
    volume_potential(const boost::optional<volumedriver::ClusterSize>&,
                     const boost::optional<volumedriver::SCOMultiplier>&,
                     const boost::optional<volumedriver::TLogMultiplier>&);

    uint64_t
    volume_potential(const ObjectId&);

    using MaybeFailOverCacheConfig = boost::optional<volumedriver::FailOverCacheConfig>;

    void
    set_manual_foc_config(const ObjectId&,
                          const MaybeFailOverCacheConfig&);

    void
    set_manual_default_foc_config(const MaybeFailOverCacheConfig&);

    void
    set_automatic_foc_config(const ObjectId&);

    FailOverCacheConfigMode
    get_foc_config_mode(const ObjectId& oid);

    MaybeFailOverCacheConfig
    get_foc_config(const ObjectId& oid);

    void
    queue_scrub_reply(const ObjectId& oid,
                      const scrubbing::ScrubReply&);

    ScrubManager&
    scrub_manager();

    FastPathCookie
    read(const FastPathCookie&,
         const ObjectId&,
         uint8_t* buf,
         size_t* size,
         off_t off);

    FastPathCookie
    write(const FastPathCookie&,
          const ObjectId&,
          const uint8_t* buf,
          size_t* size,
          off_t off,
          volumedriver::DtlInSync&);

    FastPathCookie
    sync(const FastPathCookie&,
         const ObjectId&,
         volumedriver::DtlInSync&);

    FastPathCookie
    fast_path_cookie(const Object&);

    // This isn't a VolumeDriverComponent, only part of one.
    void
    update_config(const boost::property_tree::ptree& pt,
                  volumedriver::UpdateReport& rep);

    void
    persist_config(boost::property_tree::ptree& pt,
                   const ReportDefault report_default) const;

private:
    DECLARE_LOGGER("VFSLocalNode");

    DECLARE_PARAMETER(vrouter_local_io_sleep_before_retry_usecs);
    DECLARE_PARAMETER(vrouter_local_io_retries);
    DECLARE_PARAMETER(vrouter_sco_multiplier);
    DECLARE_PARAMETER(vrouter_lock_reaper_interval);
    DECLARE_PARAMETER(vrouter_backend_sync_check_interval_ms);
    DECLARE_PARAMETER(scrub_manager_interval);
    DECLARE_PARAMETER(scrub_manager_sync_wait_secs);

    std::unique_ptr<filedriver::ContainerManager> fdriver_;

    // Per-object locks - to be used in exclusive mode (W) on
    // * volume / file destruction
    // * snapshot rollback
    // * conversion to template
    // * restart
    // .
    // During all other calls shared mode (R) is preferrable to allow concurrent I/O.
    //
    // The object_lock_map_lock_ is unsurprisingly only used to protect the
    // object_lock_map_.
    // Lock order: per-object lock before VolumeDriver management lock.
    typedef std::shared_ptr<fungi::RWLock> RWLockPtr;
    typedef std::map<ObjectId, RWLockPtr> ObjectLockMap;
    ObjectLockMap object_lock_map_;

    // make it a RWLock (again) if profiling shows its a bottleneck
    mutable std::mutex object_lock_map_lock_;

    // A unique_ptr so it can be reset if the interval - which is typically very big -
    // is updated.
    // XXX: Enhance the PeriodicAction interface instead.
    std::unique_ptr<youtils::PeriodicAction> lock_reaper_;

    std::unique_ptr<ScrubManager> scrub_manager_;

    void
    reset_lock_reaper_();

    void
    reap_locks_();

    RWLockPtr
    get_lock_(const ObjectId& id);

    template<typename Ret, typename... Args>
    Ret
    with_volume_pointer_(Ret (LocalNode::*fn)(volumedriver::WeakVolumePtr,
                                              Args...),
                         const ObjectId&,
                         Args...);

    template<typename Ret, typename... Args>
    Ret
    maybe_retry_(Ret (*fn)(Args...),
                 Args... args)
    {
        uint64_t attempt = 0;
        const auto retries = vrouter_local_io_retries.value();

        while (true)
        {
            LOG_TRACE("attempt " << attempt << " of " << retries);

            try
            {
                return (*fn)(args...);
            }
            catch (volumedriver::TransientException& e)
            {
                LOG_TRACE("caught transient exception " << e.what() << ",  attempt " <<
                          attempt << " of " << retries);
                if (attempt < retries)
                {
                    // XXX:
                    // * This will block the whole filesystem if running in single threaded
                    //   mode!
                    // * Figure out whether returning EAGAIN in the originating call
                    //   leads to FUSE or the kernel retrying
                    ++attempt;
                    const auto t(std::chrono::microseconds(vrouter_local_io_sleep_before_retry_usecs.value()));
                    std::this_thread::sleep_for(t);
                }
                else
                {
                    throw;
                }
            }
        }
    }

    void
    read_(volumedriver::WeakVolumePtr vol,
          uint8_t* buf,
          size_t* size,
          off_t off);

    FastPathCookie
    write_(const FastPathCookie&,
           const ObjectId&,
           const uint8_t* buf,
           size_t* size,
           off_t off,
           volumedriver::DtlInSync&);

    void
    write_(volumedriver::WeakVolumePtr,
           const uint8_t* buf,
           size_t size,
           off_t off,
           volumedriver::DtlInSync&);

    void
    sync_(volumedriver::WeakVolumePtr,
          volumedriver::DtlInSync&);

    uint64_t
    get_size_(volumedriver::WeakVolumePtr vol);

    volumedriver::ClusterMultiplier
    get_cluster_multiplier_(volumedriver::WeakVolumePtr vol);

    volumedriver::CloneNamespaceMap
    get_clone_namespace_map_(volumedriver::WeakVolumePtr vol);

    std::vector<volumedriver::ClusterLocation>
    get_page_(volumedriver::WeakVolumePtr vol,
              const volumedriver::ClusterAddress);

    void
    resize_(volumedriver::WeakVolumePtr vol,
            uint64_t newsize);

    void
    destroy_(volumedriver::WeakVolumePtr,
             volumedriver::DeleteLocalData,
             volumedriver::RemoveVolumeCompletely,
             Deadline);

    void
    do_adjust_failovercache_config_(const volumedriver::VolumeId&,
                                    const MaybeFailOverCacheConfig&);

    void
    adjust_failovercache_config_(const ObjectId& volume_id,
                                 const FailOverCacheConfigMode&,
                                 const MaybeFailOverCacheConfig&);

    void
    try_adjust_failovercache_config_(const ObjectId& id);

    void
    create_volume_(const ObjectId& id,
                   std::unique_ptr<volumedriver::MetaDataBackendConfig> mdb_config);

    void
    create_file_(const Object& obj);

    void
    remove_local_volume_data_(const ObjectId& id);

    void
    local_restart_volume_(const ObjectRegistration& reg,
                          ForceRestart force);

    void
    restart_volume_from_backend_(const ObjectId& id,
                                 ForceRestart force);

    void
    stop_volume_(const ObjectId& id,
                 volumedriver::DeleteLocalData);

    boost::optional<backend::Garbage>
    apply_scrub_reply_(const ObjectId& oid,
                       const scrubbing::ScrubReply&,
                       const volumedriver::ScrubbingCleanup);

    void
    collect_scrub_garbage_(backend::Garbage);

    template<typename ReturnType,
             typename... Args>
    ReturnType
    convert_fdriver_exceptions_(ReturnType (filedriver::ContainerManager::*mem_fun)(const filedriver::ContainerId&,
                                                                                    Args...),
                                const Object&,
                                Args...);
};

}

#endif // !VFS_LOCAL_NODE_H_
