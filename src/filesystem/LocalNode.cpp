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

#include "LocalNode.h"
#include "ObjectRegistry.h"
#include "ObjectRouter.h"
#include "TracePoints_tp.h"
#include "CloneFileFlags.h"

#include <mutex>
#include <set>

#include <boost/property_tree/ptree.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/ScopeExit.h>
#include <youtils/Timer.h>
#include <youtils/IOException.h>

#include <volumedriver/Api.h>
#include <volumedriver/TransientException.h>
#include <volumedriver/VolManager.h>
#include <volumedriver/VolumeConfig.h>

namespace volumedriverfs
{

namespace ara = arakoon;
namespace be = backend;
namespace bpt = boost::property_tree;
namespace fd = filedriver;
namespace vd = volumedriver;
namespace yt = youtils;

#define LOCKVD()                                                        \
    std::lock_guard<fungi::Mutex> vdg__(api::getManagementMutex())

#define LOCK_LOCKS()                                                    \
    std::lock_guard<decltype(object_lock_map_lock_)> vlmlg_(object_lock_map_lock_)

const uint32_t LocalNode::cached_md_pages_ = 8 * 1024;

#define CONTAINER_ID(obj)                       \
    static_cast<fd::ContainerId>(obj.id)

namespace
{

bool
is_file(const Object& o)
{
    return o.type == ObjectType::File;
}

void
with_api_exception_conversion(std::function<void()>&& fn)
{
    try
    {
        LOCKVD();
        fn();
    }
    catch (vd::VolManager::VolumeDoesNotExistException& e)
    {
        throw ObjectNotRunningHereException(e.what());
    }
}

}

LocalNode::LocalNode(ObjectRouter& router,
                     const ClusterNodeConfig& cfg,
                     const bpt::ptree& pt)
    : ClusterNode(router, cfg)
    , vrouter_local_io_sleep_before_retry_usecs(pt)
    , vrouter_local_io_retries(pt)
    , vrouter_sco_multiplier(pt)
    , vrouter_lock_reaper_interval(pt)
{
    LOG_TRACE("Initializing volumedriver");

    api::Init(pt,
              router.event_publisher());

    fdriver_.reset(new fd::ContainerManager(api::backend_connection_manager(),
                                            pt));

    reset_lock_reaper_();
}

LocalNode::~LocalNode()
{
    api::Exit();
}

void
LocalNode::destroy(ObjectRegistry& registry,
                   const bpt::ptree& pt)
{
    auto cm(be::BackendConnectionManager::create(pt, RegisterComponent::F));

    TODO("AR: push volume namespace removal down to voldrv");
    fd::ContainerManager::destroy(cm, pt);

    const std::list<ObjectId> l(registry.list());
    for (const auto& o : l)
    {
        const ObjectRegistrationPtr reg(registry.find(o));

        // non-volume objects were already nuked by ContainerManager::destroy
        if (reg->treeconfig.object_type != ObjectType::File)
        {
            // XXX: this needs to be pushed down to volumedriver
            LOG_INFO("Leftover volume " << reg->volume_id <<
                     " - removing backend namespace");
            try
            {
                cm->newBackendInterface(reg->getNS())->deleteNamespace();
            }
            catch (be::BackendNamespaceDoesNotExistException& e)
            {
                LOG_WARN("Namespace " << reg->getNS() <<
                         " does not exist anymore - proceeding");
            }
        }

        registry.wipe_out(reg->volume_id);
    }
}

void
LocalNode::update_config(const bpt::ptree& pt,
                         vd::UpdateReport& rep)
{
#define U(val)                                  \
    val.update(pt, rep)

    U(vrouter_local_io_sleep_before_retry_usecs);
    U(vrouter_local_io_retries);
    U(vrouter_sco_multiplier);

    const uint32_t old_reaper_interval = vrouter_lock_reaper_interval.value();
    U(vrouter_lock_reaper_interval);

    if (vrouter_lock_reaper_interval.value() != old_reaper_interval)
    {
        reset_lock_reaper_();
    }

#undef U
}

void
LocalNode::persist_config(bpt::ptree& pt,
                          const ReportDefault report_default) const
{
#define P(var)                                  \
    (var).persist(pt, report_default);

    P(vrouter_local_io_sleep_before_retry_usecs);
    P(vrouter_local_io_retries);
    P(vrouter_sco_multiplier);

#undef P
}

void
LocalNode::reset_lock_reaper_()
{
    lock_reaper_.reset(new yt::PeriodicAction("VolumeLockReaper",
                                              [this]
                                              {
                                                  reap_locks_();
                                              },
                                              vrouter_lock_reaper_interval.value()));
}

void
LocalNode::reap_locks_()
{
    LOCK_LOCKS();
    LOG_TRACE("dropping unused locks");

    // instead of messing with possibly invalid iterators, we simply swap
    ObjectLockMap vlm;

    for (auto& v : object_lock_map_)
    {
        if (v.second.use_count() > 1)
        {
            vlm.insert(v);
        }
    }

    std::swap(vlm, object_lock_map_);
}

LocalNode::RWLockPtr
LocalNode::get_lock_(const ObjectId& id)
{
    LOCK_LOCKS();

    auto it = object_lock_map_.find(id);
    if (it != object_lock_map_.end())
    {
        return it->second;
    }
    else
    {
        RWLockPtr l(new fungi::RWLock(id.str()));
        object_lock_map_[id] = l;
        return l;
    }
}

template<typename R,
         typename... A>
R
LocalNode::with_volume_pointer_(R (LocalNode::*fn)(vd::Volume*,
                                                   A... args),
                                const ObjectId& id,
                                A... args)
{
    vd::Volume* vol;
    {
        LOCKVD();
        vol = api::getVolumePointer(static_cast<vd::VolumeId>(id));
    }

    VERIFY(vol);
    return (this->*fn)(vol,
                       std::forward<A>(args)...);
}

template<typename... A>
void
LocalNode::maybe_retry_(void (*fn)(A... args),
                        A... args)
{
    uint64_t attempt = 0;
    const auto retries = vrouter_local_io_retries.value();

    while (true)
    {
        LOG_TRACE("attempt " << attempt << " of " << retries);

        try
        {
            (*fn)(args...);
            return;
        }
        catch (vd::TransientException& e)
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
LocalNode::read(const Object& obj,
                uint8_t* buf,
                size_t* size,
                off_t off)
{
    tracepoint(openvstorage_filesystem,
	       local_object_read_start,
	       obj.id.str().c_str(),
	       off,
	       *size);

    auto on_exit(yt::make_scope_exit([&]
				     {
				       tracepoint(openvstorage_filesystem,
						  local_object_read_end,
						  obj.id.str().c_str(),
						  off,
						  *size,
						  std::uncaught_exception());
				     }));

    ASSERT(size);

    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);

    if (is_file(obj))
    {
        *size = fdriver_->read(CONTAINER_ID(obj),
                               off,
                               buf,
                               *size);
    }
    else
    {
        with_volume_pointer_(&LocalNode::read_,
                             obj.id,
                             buf,
                             size,
                             off);
    }
}

// TODO: align I/O to cluster instead of LBA size to avoid the volumedriver lib
// having to do it internally.
void
LocalNode::read_(vd::Volume* vol,
                 uint8_t* buf,
                 size_t* size,
                 const off_t off)
{
    ASSERT(vol != nullptr);

    const uint64_t volume_size = api::GetSize(vol);
    if (off >= static_cast<decltype(off)>(volume_size))
    {
        *size = 0;
        return;
    }

    const uint64_t lbasize = api::GetLbaSize(vol);
    const uint64_t lba = off / lbasize;
    const uint64_t lbaoff = off % lbasize;

    uint64_t to_read = std::min(volume_size-off, *size);
    uint64_t rsize = to_read;

    bool unaligned = lbaoff != 0;

    if (unaligned)
    {
        rsize += lbaoff;
    }

    if (rsize % lbasize)
    {
        unaligned = true;
        rsize += lbasize - (rsize % lbasize);
    }

    LOG_TRACE("reading, off " << (lba * lbasize) << " (LBA " << lba <<
              "), size " << rsize << ", unaligned " << unaligned);

    if (unaligned)
    {
        std::vector<uint8_t> bounce_buf(rsize);
        maybe_retry_(&api::Read,
                     vol,
                     lba,
                     bounce_buf.data(),
                     bounce_buf.size());

        memcpy(buf, bounce_buf.data() + lbaoff, to_read);
    }
    else
    {
        ASSERT(to_read == rsize);
        maybe_retry_(&api::Read,
                     vol,
                     lba,
                     buf,
                     to_read);
    }
    *size = to_read;
}

void
LocalNode::write(const Object& obj,
                 const uint8_t* buf,
                 size_t* size,
                 off_t off)
{
    ASSERT(size);

    tracepoint(openvstorage_filesystem,
	       local_object_write_start,
	       obj.id.str().c_str(),
	       off,
	       *size);

    auto on_exit(yt::make_scope_exit([&]
				     {
				       tracepoint(openvstorage_filesystem,
						  local_object_write_end,
						  obj.id.str().c_str(),
						  off,
						  *size,
						  std::uncaught_exception());
				     }));

    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);

    if (is_file(obj))
    {
        *size = fdriver_->write(CONTAINER_ID(obj),
                                off,
                                buf,
                                *size);
    }
    else
    {
        with_volume_pointer_(&LocalNode::write_,
                             obj.id,
                             buf,
                             *size,
                             off);
    }
}

// TODO: align I/O to cluster instead of LBA size to avoid the volumedriver lib
// having to do it internally.
void
LocalNode::write_(vd::Volume* vol,
                  const uint8_t* buf,
                  const size_t size,
                  const off_t off)
{
    ASSERT(vol != nullptr);

    const uint64_t lbasize = api::GetLbaSize(vol);
    const uint64_t lba = off / lbasize;
    const uint64_t lbaoff = off % lbasize;
    uint64_t wsize = size;
    bool unaligned = lbaoff != 0;

    if (unaligned)
    {
        wsize += off % lbasize;
    }

    if (wsize % lbasize)
    {
        unaligned = true;
        wsize += lbasize - wsize % lbasize;
    }

    LOG_TRACE("writing, off " << (lba * lbasize) << " (LBA " << lba <<
              "), size " << wsize << ", unaligned " << unaligned);

    typedef void (Writer)(vd::Volume*,
                          uint64_t,
                          const uint8_t*,
                          uint64_t);
    // Const casts here are necessary
    if (unaligned)
    {
        std::vector<uint8_t> bounce_buf(wsize);

        maybe_retry_(&api::Read,
                     vol,
                     const_cast<uint64_t&>(lba),
                     bounce_buf.data(),
                     bounce_buf.size());

        memcpy(bounce_buf.data() + lbaoff, buf, size);

        maybe_retry_(static_cast<Writer*>(&api::Write),
                     vol,
                     lba,
                     static_cast<const unsigned char*>(bounce_buf.data()),
                     wsize);
    }
    else
    {
        maybe_retry_(static_cast<Writer*>(&api::Write),
                     vol,
                     lba,
                     buf,
                     wsize);
    }
}

void
LocalNode::sync(const Object& obj)
{
    tracepoint(openvstorage_filesystem,
	       local_object_sync_start,
	       obj.id.str().c_str());

    auto on_exit(yt::make_scope_exit([&]
				     {
				       tracepoint(openvstorage_filesystem,
						  local_object_sync_end,
						  obj.id.str().c_str(),
						  std::uncaught_exception());
				     }));

    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);

    if (is_file(obj))
    {
        fdriver_->sync(CONTAINER_ID(obj));
    }
    else
    {
        with_volume_pointer_(&LocalNode::sync_,
                             obj.id);
    }
}

void
LocalNode::sync_(vd::Volume* vol)
{
    api::Sync(vol);
}

uint64_t
LocalNode::get_size(const Object& obj)
{
    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);

    if (is_file(obj))
    {
        return fdriver_->size(CONTAINER_ID(obj));
    }
    else
    {
        return with_volume_pointer_(&LocalNode::get_size_,
                                    obj.id);
    }
}

uint64_t
LocalNode::get_size_(vd::Volume* vol)
{
    return api::GetSize(vol);
}

void
LocalNode::resize(const Object& obj,
                  uint64_t newsize)
{
    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);

    if (is_file(obj))
    {
        fdriver_->resize(CONTAINER_ID(obj),
                         newsize);
    }
    else
    {
        with_volume_pointer_(&LocalNode::resize_,
                             obj.id,
                             newsize);
    }
}

void
LocalNode::resize_(vd::Volume* vol,
                   uint64_t newsize)
{
    LOCKVD();

    const uint64_t csize = api::GetClusterSize();
    uint64_t clusters = newsize / csize;
    if (newsize % csize)
    {
        LOG_WARN("new size " << newsize << " not aligned to cluster size " <<
                 csize << " - trying best effort");
        ++clusters;
    }

    api::Resize(vol, clusters);
}

void
LocalNode::unlink(const Object& obj)
{
    LOG_INFO(obj.id << ": deleting object");

    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedWriteLock wg(*l);

    LOG_TRACE(obj.id << ": unregistering");
    ObjectRegistrationPtr reg(vrouter_.object_registry()->find(obj.id,
                                                               IgnoreCache::T));

    if (reg)
    {
        try
        {
            vrouter_.object_registry()->unregister(obj.id);
        }
        catch (WrongOwnerException& e)
        {
            throw ObjectNotRunningHereException(e.what());
        }
    }

    TODO("AR: if anything below throws we're in for inconsistencies");

    if (is_file(obj))
    {
        fdriver_->unlink(CONTAINER_ID(obj));
    }
    else
    {
        with_volume_pointer_(&LocalNode::destroy_,
                             obj.id,
                             vd::DeleteLocalData::T,
                             vd::RemoveVolumeCompletely::T,
                             MaybeSyncTimeoutMilliSeconds());
    }
}

void
LocalNode::destroy_(vd::Volume* vol,
                    vd::DeleteLocalData delete_local,
                    vd::RemoveVolumeCompletely remove_completely,
                    MaybeSyncTimeoutMilliSeconds maybe_sync_timeout_ms)
{
    const vd::VolumeId id(api::getVolumeId(vol));

    LOG_TRACE(id << ": delete_local " << delete_local <<
              ", remove_completely: " << remove_completely);

    ASSERT(vol != nullptr);
    get_lock_(static_cast<const ObjectId>(id))->assertWriteLocked();

    yt::SteadyTimer timer;

    // TODO: this is a bandaid that needs to go - with a FOC or a yet to be invented
    // mechanism there's no need to schedule a backendsync; and we also want to be
    // able to get out of this.
    if (remove_completely == vd::RemoveVolumeCompletely::F and
        delete_local == vd::DeleteLocalData::T)
    {
        {
            LOCKVD();
            LOG_INFO(vol->getName() << ": trying to sync to the backend");
            api::scheduleBackendSync(id);
        }

        while (true)
        {
            if (maybe_sync_timeout_ms and
                timer.elapsed() > *maybe_sync_timeout_ms)
            {
                LOG_ERROR(vol->getName() <<
                          ": timeout syncing to the backend: " <<
                          timer.elapsed() << " > " << *maybe_sync_timeout_ms);
                throw SyncTimeoutException("Timeout syncing volume to backend",
                                           vol->getName().c_str());
            }

            bool synced = false;
            LOCKVD();
            synced = api::isVolumeSynced(id);
            if (synced)
            {
                LOG_INFO(vol->getName() << ": synced to the backend");
                break;
            }
            else
            {
                boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
            }
        }
    }

    LOCKVD();
    // THis  never worked
    api::destroyVolume(vol,
                       delete_local,
                       remove_completely,
                       T(remove_completely) ?
                       vd::DeleteVolumeNamespace::T :
                       vd::DeleteVolumeNamespace::F,
                       vd::ForceVolumeDeletion::F);
}

void
LocalNode::remove_local_volume_data_(const ObjectId& id)
{
    RWLockPtr l(get_lock_(id));
    fungi::ScopedWriteLock wg(*l);

    LOCKVD();
    api::removeLocalVolumeData(be::Namespace(id.str()));
}

void
LocalNode::remove_local_data(const Object& obj)
{
    LOG_TRACE(obj);

    if (is_file(obj))
    {
        fdriver_->drop_from_cache(CONTAINER_ID(obj));
    }
    else
    {
        remove_local_volume_data_(obj.id);
    }
}

void
LocalNode::local_restart_volume_(const ObjectRegistration& reg,
                                 ForceRestart force)
{
    const ObjectId& id = reg.volume_id;

    try
    {
        LOCKVD();

        if (not api::get_volume_pointer_no_throw(static_cast<const vd::VolumeId>(id)))
        {
            api::local_restart(reg.getNS(),
                               reg.owner_tag,
                               vd::FallBackToBackendRestart::T,
                               force == ForceRestart::T ?
                               vd::IgnoreFOCIfUnreachable::T :
                               vd::IgnoreFOCIfUnreachable::F,
                               cached_md_pages_);
        }
        else
        {
            LOG_WARN(id << " already running");
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(id << ": failed to restart volume: " << EWHAT);
            throw;
        });

    // At this point the volume restart is considered to be successful.
    // Failure to configure the failovercache correctly should not fail the restart
    try_adjust_failovercache_config_(id);
}

void
LocalNode::local_restart(const ObjectRegistration& reg,
                         ForceRestart force)
{
    const ObjectId& id = reg.volume_id;

    LOG_TRACE(id);

    RWLockPtr l(get_lock_(id));
    fungi::ScopedWriteLock wg(*l);

    if (is_file(reg.object()))
    {
        fdriver_->restart(static_cast<fd::ContainerId>(id));
    }
    else
    {
        local_restart_volume_(reg,
                              force);
    }
}

void
LocalNode::restart_volume_from_backend_(const ObjectId& id,
                                        ForceRestart force)
{
    try
    {
        // The restart_volume_from_backend is currently only used within a transfer
        // workflow. The previous owner should have updated the registry to make us
        // owner of the volume.
        // We check the registry while holding the volume_lock to prevent e.g. a
        // concurrent transfer action from giving away the volume again.
        const ObjectRegistrationPtr
            reg(vrouter_.object_registry()->find_throw(id,
                                                       IgnoreCache::T));

        if(reg->node_id != vrouter_.node_id())
        {
            std::stringstream ss;
            ss << "restarting volume " << id << " on node " << vrouter_.node_id() <<
                " but found other owner " << reg->node_id;
            throw OwnershipException(ss.str().c_str(),
                                     id.str().c_str());
        }

        LOCKVD();

        if (not api::get_volume_pointer_no_throw(static_cast<const vd::VolumeId>(id)))
        {
            api::backend_restart(be::Namespace(id.str()),
                                 reg->owner_tag,
                                 vd::PrefetchVolumeData::F,
                                 force == ForceRestart::T ?
                                 vd::IgnoreFOCIfUnreachable::T :
                                 vd::IgnoreFOCIfUnreachable::F,
                                 cached_md_pages_);
        }
        else
        {
            LOG_WARN(id << " already running");
        }
    }
    CATCH_STD_ALL_LOG_RETHROW(id << ": failed to restart volume: ");

    try_adjust_failovercache_config_(id);
}

void
LocalNode::backend_restart(const Object& obj,
                           ForceRestart force,
                           PrepareRestartFun prep_restart_fun)
{
    LOG_TRACE(obj << ": attempting restart from backend");

    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedWriteLock wg(*l);

    prep_restart_fun(obj);

    if (is_file(obj))
    {
        fdriver_->restart(CONTAINER_ID(obj));
    }
    else
    {
        restart_volume_from_backend_(obj.id,
                                     force);
    }
}

void
LocalNode::create(const Object& obj,
                  vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config)
{
    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedWriteLock wg(*l);

    if (is_file(obj))
    {
        create_file_(obj);
    }
    else
    {
        create_volume_(obj.id,
                       std::move(mdb_config));
    }
}

void
LocalNode::create_file_(const Object& obj)
{
    const ObjectId& id = obj.id;

    vrouter_.object_registry()->register_file(id);

    try
    {
        fdriver_->create(CONTAINER_ID(obj));
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR("Failed to create " << obj << ": " << EWHAT);
            vrouter_.object_registry()->unregister(id);
            throw;
        });
}

void
LocalNode::create_volume_(const ObjectId& id,
                          vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config)
{
    const be::Namespace nspace(id.str());

    try
    {
        ObjectRegistrationPtr
            reg(vrouter_.object_registry()->register_base_volume(id,
                                                                 nspace));

        LOCKVD();

        try
        {
            const auto
                params(vd::VanillaVolumeConfigParameters(static_cast<const vd::VolumeId>(id),
                                                         nspace,
                                                         vd::VolumeSize(0),
                                                         reg->owner_tag)
                       .sco_multiplier(vd::SCOMultiplier(vrouter_sco_multiplier.value()))
                       .metadata_cache_pages(cached_md_pages_)
                       .metadata_backend_config(std::move(mdb_config)));

            api::createNewVolume(params,
                                 vd::CreateNamespace::T);
        }
        CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to create volume " << id << ": " << EWHAT);
                vrouter_.object_registry()->unregister(id);
                throw;
            });
    }
    CATCH_STD_ALL_EWHAT({
            LOG_ERROR(id << ": failed to create volume: " << EWHAT);
            throw;
        });

    // At this point volume creation is considered to be successful.
    // Failure to configure the failovercache correctly should not fail the create call.

    try_adjust_failovercache_config_(id);
}

void
LocalNode::create_clone(const ObjectId& clone_id,
                        vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config,
                        const ObjectId& parent_id,
                        const MaybeSnapshotName& maybe_parent_snap)
{
    const be::Namespace clone_nspace(clone_id.str());

    bool registered = false;

    try
    {
        RWLockPtr l(get_lock_(clone_id));
        fungi::ScopedWriteLock wg(*l);

        ObjectRegistrationPtr
            reg(vrouter_.object_registry()->register_clone(clone_id,
                                                           clone_nspace,
                                                           parent_id,
                                                           maybe_parent_snap));

        registered = true;

        // We're not in the hot data path so it's ok to fetch the registration
        // from the registry's backend and not the cache.
        ObjectRegistrationPtr
            parent_reg(vrouter_.object_registry()->find_throw(parent_id,
                                                              IgnoreCache::T));

        LOCKVD();

        auto params =
            vd::CloneVolumeConfigParameters(static_cast<const vd::VolumeId>(clone_id),
                                            clone_nspace,
                                            parent_reg->getNS(),
                                            reg->owner_tag)
            .metadata_cache_pages(cached_md_pages_)
            .metadata_backend_config(std::move(mdb_config))
            ;
        if (maybe_parent_snap)
        {
            params.parent_snapshot(*maybe_parent_snap);
        }

        api::createClone(params,
                         vd::PrefetchVolumeData::F,
                         vd::CreateNamespace::T);
    }
    CATCH_STD_ALL_EWHAT({
                LOG_ERROR("Failed to create a clone from " << parent_id <<
                          ", snapshot " << maybe_parent_snap << EWHAT);

                if (registered)
                {
                    vrouter_.object_registry()->unregister(clone_id);
                }
                throw;
        });

    // At this point the volume clone call is considered to be successful.
    // Failure to configure the failovercache correctly should not fail the clone call.
    try_adjust_failovercache_config_(clone_id);
}

void
LocalNode::clone_to_existing_volume(const ObjectId& clone_id,
                                    vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config,
                                    const ObjectId& parent_id,
                                    const MaybeSnapshotName& maybe_parent_snap)
{
    RWLockPtr l(get_lock_(clone_id));
    fungi::ScopedWriteLock wg(*l);
    /*destroy the volume without deleting the namespace*/
    with_volume_pointer_(&LocalNode::destroy_,
                         clone_id,
                         vd::DeleteLocalData::T,
                         vd::RemoveVolumeCompletely::F,
                         MaybeSyncTimeoutMilliSeconds());
    /*clear the clone namespace*/
    auto cm(api::backend_connection_manager());
    const ObjectRegistrationPtr
        parent_reg(vrouter_.object_registry()->find_throw(parent_id,
                                                          IgnoreCache::T));
    const ObjectRegistrationPtr
        clone_reg(vrouter_.object_registry()->find_throw(clone_id,
                                                         IgnoreCache::T));
    cm->newBackendInterface(clone_reg->getNS())->clearNamespace();
    /*create a clone in that namespace*/
    const be::Namespace clone_nspace(clone_id.str());
    LOCKVD();
    auto params =
        vd::CloneVolumeConfigParameters(static_cast<const vd::VolumeId>(clone_id),
                                        clone_nspace,
                                        parent_reg->getNS(),
                                        clone_reg->owner_tag)
        .metadata_cache_pages(cached_md_pages_)
        .metadata_backend_config(std::move(mdb_config))
        ;
    if (maybe_parent_snap)
    {
        params.parent_snapshot(*maybe_parent_snap);
    }

    api::createClone(params,
                     vd::PrefetchVolumeData::F,
                     vd::CreateNamespace::F);
    /*convert old base volume registration to clone*/
    vrouter_.object_registry()->convert_base_to_clone(clone_id,
                                                      clone_nspace,
                                                      parent_id,
                                                      maybe_parent_snap);
}

void
LocalNode::vaai_copy(const ObjectId& src_id,
                     const boost::optional<ObjectId>& maybe_dst_id,
                     vd::VolumeConfig::MetaDataBackendConfigPtr mdb_config,
                     const uint64_t timeout,
                     const CloneFileFlags flags,
                     VAAICreateCloneFun fun)
{
    const bool is_lazy = flags & CloneFileFlags::Lazy;
    const bool is_guarded = flags & CloneFileFlags::Guarded;
    const bool is_skipzeroes = flags & CloneFileFlags::SkipZeroes;

    vd::SnapshotName snapname;
    const vd::VolumeId src_volid(src_id);
    {
        fungi::ScopedLock l(api::getManagementMutex());
        snapname = api::createSnapshot(src_volid);
    }

    namespace pt = boost::posix_time;
    pt::ptime end = pt::second_clock::local_time() + pt::seconds(timeout);

    bool synced = false;
    while (pt::second_clock::local_time() < end)
    {
        {
            fungi::ScopedLock l(api::getManagementMutex());
            synced = api::isVolumeSyncedUpTo(src_volid,
                                             snapname);
        }
        if (synced)
        {
            break;
        }
        else
        {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
        }
    }

    const vd::SnapshotName snap(snapname);
    if (not synced)
    {
        vrouter_.delete_snapshot(src_id, snap);
        throw fungi::IOException("timeout exceeded\n");
    }

    if (is_lazy && is_guarded)
    {
        fun(MaybeSnapshotName(snap),
            std::move(mdb_config));
    }
    else if (not is_lazy && not is_guarded && is_skipzeroes)
    {
        if (vrouter_.get_size(src_id) != vrouter_.get_size(*maybe_dst_id))
        {
            throw InvalidOperationException() <<
                error_desc("source and target volume size mismatch");
        }
        vrouter_.clone_to_existing_volume(vd::VolumeId(*maybe_dst_id),
                                          std::move(mdb_config),
                                          vd::VolumeId(src_id),
                                          MaybeSnapshotName(snap));
    }
    else
    {
        vrouter_.delete_snapshot(src_id,
                                 snap);
        throw InvalidOperationException() <<
            error_desc("unknown volume-based VAAI call");
    }
}

/*source and destination objects should exist already */
void
LocalNode::vaai_filecopy(const ObjectId& src_id,
                         const ObjectId& dst_id)
{
    const Object src_obj(ObjectType::File,
                         src_id);
    const Object dst_obj(ObjectType::File,
                         dst_id);
    uint64_t bytes_to_copy = get_size(src_obj);
    size_t buf_size = 1024;
    off_t offset = 0;
    while (bytes_to_copy > 0)
    {
        uint8_t buf[buf_size];
        size_t read_size = buf_size;

        read(src_obj,
             buf,
             &read_size,
             offset);

        size_t write_size = read_size;
        write(dst_obj,
              buf,
              &write_size,
              offset);
        if (write_size != read_size)
        {
            throw fungi::IOException("Couldn't write whole buffer");
        }
        bytes_to_copy -= write_size;
        offset += write_size;
    }
}

void
LocalNode::set_volume_as_template(const ObjectId& id)
{
    LOG_INFO("Set volume " << id << " as template");

    RWLockPtr l(get_lock_(id));
    fungi::ScopedWriteLock wg(*l);

    try
    {
        vrouter_.object_registry()->set_volume_as_template(id);
    }
    catch(WrongOwnerException& e)
    {
        //exception translation to feed the XMLRPC-redirector correctly
        throw ObjectNotRunningHereException(e.what());
    }

    // Even if the following fails for some reason, we don't want to
    // rollback the registry, but rather proceed/retry the setAsTemplate
    // at some later point.

    // We don't translate VolumeDoesNotExistExceptions to
    // ObjectNotRunningHereException at this point because
    //according to the registry the volume should be running here.
    LOCKVD();
    api::setAsTemplate(static_cast<const vd::VolumeId>(id));
}

void
LocalNode::rollback_volume(const ObjectId& id,
                           const vd::SnapshotName& snap_id)
{
    RWLockPtr l(get_lock_(id));
    fungi::ScopedWriteLock wg(*l);

    ObjectRegistrationPtr reg(vrouter_.object_registry()->find_throw(id, IgnoreCache::T));

    with_api_exception_conversion([&]
    {
        const vd::VolumeId vid(static_cast<const vd::VolumeId>(id));

        std::list<vd::SnapshotName> snaps;
        api::showSnapshots(vid,
                           snaps);

        std::set<vd::SnapshotName> doomed_snaps;
        bool seen = false;
        for (const auto& s : snaps)
        {
            if (s == snap_id)
            {
                seen = true;
                continue;
            }

            if (seen)
            {

                doomed_snaps.insert(s);
            }
        }

        for (const auto& entry : reg->treeconfig.descendants)
        {
            const MaybeSnapshotName& msnap = entry.second;
            if (msnap)
            {
                if (doomed_snaps.find(*msnap) != doomed_snaps.end())
                {
                    LOG_ERROR(id << ": cannot roll back to snapshot " << snap_id <<
                              " as the more recent snapshot " << *msnap <<
                              " is still required by at least " << entry.first);
                    throw ObjectStillHasChildrenException("More recent snapshot still has children",
                                                          msnap->str().c_str());
                }
            }
        }

        api::restoreSnapshot(vid,
                             snap_id);
    });
}

void
LocalNode::delete_snapshot(const ObjectId& id,
                           const vd::SnapshotName& snap_id)
{
    LOG_INFO(id << ": deleting snapshot " << snap_id);

    // not strictly necessary?
    RWLockPtr l(get_lock_(id));
    fungi::ScopedReadLock rg(*l);

    ObjectRegistrationPtr reg(vrouter_.object_registry()->find_throw(id, IgnoreCache::T));
    if (reg->node_id != vrouter_.node_id())
    {
        LOG_WARN(id << ": not running here (" << vrouter_.node_id() << ") but on " <<
                 reg->node_id);
        throw ObjectNotRunningHereException("object not running here",
                                            id.str().c_str());
    }

    for (const auto& entry : reg->treeconfig.descendants)
    {
        const MaybeSnapshotName& msnap = entry.second;
        if (msnap and *msnap == snap_id)
        {
            LOG_ERROR(id << ": cannot delete snapshot " << snap_id <<
                      " as it is still required by at least " << entry.first);
            throw ObjectStillHasChildrenException("Snapshot still has children",
                                                  snap_id.str().c_str());
        }
    }

    with_api_exception_conversion([&]
    {
        api::destroySnapshot(static_cast<const vd::VolumeId>(id),
                             snap_id);
    });
}

void
LocalNode::scrub_wrapper_(const char* desc,
                          const ObjectId& id,
                          std::function<void()> fun)
{
    // not strictly necessary?
    RWLockPtr l(get_lock_(id));
    fungi::ScopedReadLock rg(*l);

    ObjectRegistrationPtr reg(vrouter_.object_registry()->find_throw(id, IgnoreCache::T));
    if (reg->node_id != vrouter_.node_id())
    {
        LOG_WARN(id << ": not running here (" << vrouter_.node_id() << ") but on " <<
                 reg->node_id);
        throw ObjectNotRunningHereException("object not running here",
                                            id.str().c_str());
    }

    if (not reg->treeconfig.descendants.empty())
    {
        LOG_ERROR(id << ": refusing to " << desc << " as there are clones");
        throw ObjectStillHasChildrenException("volume or template still has children",
                                              id.str().c_str());
    }

    if (reg->treeconfig.object_type != ObjectType::Volume)
    {
        LOG_ERROR(id << ": refusing to " << desc << " - invalid object type " <<
                  reg->treeconfig.object_type);
        throw InvalidOperationException() <<
            error_object_id(id) <<
            error_desc("invalid object type");
    }

    with_api_exception_conversion([&]
                                  {
                                      fun();
                                  });
}

void
LocalNode::get_scrub_work(const ObjectId& id,
                          const boost::optional<vd::SnapshotName>& start_snap,
                          const boost::optional<vd::SnapshotName>& end_snap,
                          std::vector<std::string>& work)
{
    LOG_INFO(id << ": getting scrub work, start snapshot " << start_snap <<
             ", end snapshot " << end_snap);

    scrub_wrapper_("hand out scrub work",
                   id,
                   [&]
                   {
                       api::getScrubbingWork(static_cast<const vd::VolumeId>(id),
                                             work,
                                             start_snap,
                                             end_snap);
                   });
}

void
LocalNode::apply_scrub_result(const ObjectId& id,
                              const std::string& result)
{
    LOG_INFO(id << ": applying scrub result");

    scrub_wrapper_("apply scrub result",
                   id,
                   [&]
                   {
                       api::applyScrubbingWork(static_cast<const vd::VolumeId>(id),
                                               result);
                   });
}

void
LocalNode::transfer(const Object& obj,
                    const NodeId target_node,
                    MaybeSyncTimeoutMilliSeconds maybe_sync_timeout_ms)
{
    LOG_INFO(obj << ": target_node " << target_node);

    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedWriteLock wg(*l);

    if (is_file(obj))
    {
        fdriver_->drop_from_cache(CONTAINER_ID(obj));
    }
    else
    {
        // TODO [bdv] if volume was not running anymore we should still continue with
        // the ownership transfer -> check which exception to forgive
        // TODO [bdv] verify the case where the volume is not registered anymore on
        // this node
        with_volume_pointer_(&LocalNode::destroy_,
                             obj.id,
                             vd::DeleteLocalData::T,
                             vd::RemoveVolumeCompletely::F,
                             maybe_sync_timeout_ms);
    }

    ObjectRegistrationPtr
        reg(vrouter_.object_registry()->find_throw(obj.id,
                                                   IgnoreCache::T));
    vrouter_.object_registry()->migrate(obj.id,
                                        vrouter_.node_id(),
                                        target_node);
}

void
LocalNode::adjust_failovercache_config_(const ObjectId& id,
                                        const FailOverCacheConfigMode& mode,
                                        const boost::optional<vd::FailOverCacheConfig>& config)
{
    ASSERT_LOCKABLE_LOCKED(api::getManagementMutex());

    LOG_INFO(id);

    const vd::VolumeId& vid = static_cast<const vd::VolumeId>(id);
    const boost::optional<vd::FailOverCacheConfig>
        old_config(api::getFailOverCacheConfig(vid));

    vrouter_.object_registry()->set_foc_config_mode(id,
                                                    mode);

    if (old_config != config)
    {
        LOG_INFO(id << ": setting FailOverCacheConfig from " <<
                 old_config << " -> " << config);
        try
        {
            api::setFailOverCacheConfig(vid,
                                        config);
        }
        CATCH_STD_ALL_LOG_IGNORE(id <<
                                 ": error while setting FailOverCacheConfig to " <<
                                 config);
    }
    else
    {
        LOG_INFO(id << ": FailOverCacheConfig is unchanged: " << old_config);
    }
}

void
LocalNode::try_adjust_failovercache_config_(const ObjectId& id)
{
    // Holding the per-volume lock should not be necessary here.
    // Volume could not be running, restarting, being destroyed,
    // being written to, being migrated ...
    // but VDLOCK should give enough protection and we need to deal with
    // a failing setfailover in any case.
    LOCKVD();

    try
    {
        const FailOverCacheConfigMode mode(get_foc_config_mode(id));

        if (mode == FailOverCacheConfigMode::Automatic)
        {
            adjust_failovercache_config_(id,
                                         mode,
                                         vrouter_.failoverconfig_as_it_should_be());
        }
    }
    CATCH_STD_ALL_LOG_IGNORE("failed to adjust failovercache config");
}

void
LocalNode::stop_volume_(const ObjectId& id,
                        vd::DeleteLocalData delete_local_data)
{
    RWLockPtr l(get_lock_(id));
    fungi::ScopedWriteLock wg(*l);

    with_volume_pointer_(&LocalNode::destroy_,
                         id,
                         delete_local_data,
                         vd::RemoveVolumeCompletely::F,
                         MaybeSyncTimeoutMilliSeconds());
}

void
LocalNode::stop(const Object& obj,
                vd::DeleteLocalData delete_local_data)
{
    LOG_INFO("Stopping " << obj);

    if (is_file(obj))
    {
        fdriver_->drop_from_cache(CONTAINER_ID(obj));
    }
    else
    {
        stop_volume_(obj.id,
                     delete_local_data);
    }
}

uint64_t
LocalNode::volume_potential(const boost::optional<volumedriver::SCOMultiplier>& s,
                            const boost::optional<volumedriver::TLogMultiplier>& t)
{
    return api::volumePotential(s ?
                                *s :
                                vd::SCOMultiplier(vrouter_sco_multiplier.value()),
                                t);
}

uint64_t
LocalNode::volume_potential(const ObjectId& id)
{
    return api::volumePotential(be::Namespace(id.str()));
}


void
LocalNode::set_manual_foc_config(const ObjectId& oid,
                                 const boost::optional<vd::FailOverCacheConfig>& cfg)
{
    adjust_failovercache_config_(oid,
                                 FailOverCacheConfigMode::Manual,
                                 cfg);
}

void
LocalNode::set_automatic_foc_config(const ObjectId& oid)
{
    adjust_failovercache_config_(oid,
                                 FailOverCacheConfigMode::Automatic,
                                 vrouter_.failoverconfig_as_it_should_be());
}

boost::optional<volumedriver::FailOverCacheConfig>
LocalNode::get_foc_config(const ObjectId& oid)
{
    return api::getFailOverCacheConfig(static_cast<const vd::VolumeId>(oid));
}

FailOverCacheConfigMode
LocalNode::get_foc_config_mode(const ObjectId& oid)
{
    const ObjectRegistrationPtr
        reg(vrouter_.object_registry()->find_throw(oid,
                                                   IgnoreCache::T));
    return reg->foc_config_mode;
}

}
