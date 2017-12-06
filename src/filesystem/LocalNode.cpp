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

#include "CloneFileFlags.h"
#include "FileSystemEvents.h"
#include "LocalNode.h"
#include "ObjectRegistry.h"
#include "ObjectRouter.h"
#include "ScrubManager.h"
#include "ScrubTreeBuilder.h"
#include "TracePoints_tp.h"

#include <mutex>
#include <set>
#include <chrono>

#include <boost/property_tree/ptree.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/InlineExecutor.h>
#include <youtils/ScopeExit.h>
#include <youtils/Timer.h>
#include <youtils/IOException.h>

#include <backend/GarbageCollector.h>

#include <volumedriver/Api.h>
#include <volumedriver/ClusterLocation.h>
#include <volumedriver/ScrubWork.h>
#include <volumedriver/Snapshot.h>
#include <volumedriver/VolManager.h>
#include <volumedriver/Volume.h>
#include <volumedriver/VolumeConfig.h>

namespace volumedriverfs
{

namespace ara = arakoon;
namespace bc = boost::chrono;
namespace be = backend;
namespace bpt = boost::property_tree;
namespace fd = filedriver;
namespace ph = std::placeholders;
namespace vd = volumedriver;
namespace yt = youtils;

#define LOCKVD()                                                        \
    std::lock_guard<fungi::Mutex> vdg__(api::getManagementMutex())

#define LOCK_LOCKS()                                                    \
    std::lock_guard<decltype(object_lock_map_lock_)> vlmlg_(object_lock_map_lock_)

namespace
{

bool
is_file(const Object& o)
{
    return o.type == ObjectType::File;
}

void
with_api_exception_conversion(std::function<void()> fn)
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
    : ClusterNode(router,
                  cfg.vrouter_id,
                  cfg.message_uri())
    , vrouter_local_io_sleep_before_retry_usecs(pt)
    , vrouter_local_io_retries(pt)
    , vrouter_sco_multiplier(pt)
    , vrouter_lock_reaper_interval(pt)
    , vrouter_backend_sync_check_interval_ms(pt)
    , scrub_manager_interval(pt)
    , scrub_manager_sync_wait_secs(pt)
    , scrub_manager_max_parent_scrubs(pt)
    , scrub_manager_enabled(pt)
{
    LOG_TRACE("Initializing volumedriver");

    THROW_WHEN(vrouter_sco_multiplier.value() == 0);
    THROW_WHEN(vrouter_sco_multiplier.value() >= (1U << (8 * sizeof(vd::SCOOffset))));

    api::Init(pt,
              router.event_publisher());

    auto on_error(yt::make_scope_exit_on_exception([]
                                                   {
                                                       api::Exit();
                                                   }));

    fdriver_.reset(new fd::ContainerManager(api::backend_connection_manager(),
                                            pt));

    reset_lock_reaper_();

    ObjectRegistry& reg = router.object_registry()->registry();

    scrub_manager_ =
        std::make_unique<ScrubManager>(reg,
                                       reg.locked_arakoon(),
                                       scrub_manager_interval.value(),
                                       scrub_manager_enabled.value(),
                                       std::bind(&LocalNode::apply_scrub_reply_,
                                                 this,
                                                 ph::_1,
                                                 ph::_2,
                                                 ph::_3),
                                       ScrubTreeBuilder(reg),
                                       std::bind(&LocalNode::collect_scrub_garbage_,
                                                 this,
                                                 ph::_1));
}

LocalNode::~LocalNode()
{
    scrub_manager_ = nullptr;
    api::Exit();
}

void
LocalNode::destroy(ObjectRegistry& registry,
                   const bpt::ptree& pt)
{
    ScrubManager(registry,
                 registry.locked_arakoon()).destroy();

    auto cm(be::BackendConnectionManager::create(pt, RegisterComponent::F));

    TODO("AR: push volume namespace removal down to voldrv");
    fd::ContainerManager::destroy(cm, pt);

    const std::vector<ObjectId> l(registry.list());
    for (const auto& o : l)
    {
        const ObjectRegistrationPtr reg(registry.find(o));

        // non-volume objects were already nuked by fd::ContainerManager::destroy
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

    U(vrouter_backend_sync_check_interval_ms);
    U(scrub_manager_interval);
    U(scrub_manager_sync_wait_secs);
    U(scrub_manager_max_parent_scrubs);
    U(scrub_manager_enabled);

    scrub_manager_->enable(scrub_manager_enabled.value());

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
    P(vrouter_backend_sync_check_interval_ms)
    P(scrub_manager_interval);
    P(scrub_manager_sync_wait_secs);
    P(scrub_manager_max_parent_scrubs);
    P(scrub_manager_enabled);

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
LocalNode::with_volume_pointer_(R (LocalNode::*fn)(vd::WeakVolumePtr,
                                                   A... args),
                                const ObjectId& id,
                                A... args)
{
    vd::WeakVolumePtr vol;
    {
        LOCKVD();
        vol = api::getVolumePointer(static_cast<vd::VolumeId>(id));
    }

    return (this->*fn)(vol,
                       std::forward<A>(args)...);
}

template<typename ReturnType,
         typename... Args>
ReturnType
LocalNode::convert_fdriver_exceptions_(ReturnType (fd::ContainerManager::*mem_fun)(const fd::ContainerId&,
                                                                                   Args...),
                                       const Object& obj,
                                       Args... args)
{
    try
    {
        return (fdriver_.get()->*mem_fun)(static_cast<fd::ContainerId>(obj.id),
                                          std::forward<Args>(args)...);
    }
    catch (const fd::ContainerManager::ContainerDoesNotExistException& e)
    {
        throw ObjectNotRunningHereException(e.what());
    }
}

ClusterNode::ReadFuture
LocalNode::read(const Object& obj,
                uint8_t* buf,
                size_t size,
                off_t off)
{
    tracepoint(openvstorage_filesystem,
	       local_object_read_start,
	       obj.id.str().c_str(),
	       off,
	       size);

    auto on_exc(yt::make_scope_exit_on_exception([&]
                                                 {
                                                     tracepoint(openvstorage_filesystem,
                                                                local_object_read_end,
                                                                obj.id.str().c_str(),
                                                                off,
                                                                size,
                                                                true);
                                                 }));

    ReadFuture rfuture;

    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);

    if (is_file(obj))
    {
        size = convert_fdriver_exceptions_<size_t,
                                           off_t,
                                           void*,
                                           size_t>(&fd::ContainerManager::read,
                                                   obj,
                                                   off,
                                                   buf,
                                                   size);
        rfuture = boost::make_ready_future(size);
    }
    else
    {
        rfuture = with_volume_pointer_<ReadFuture,
                                       uint8_t*,
                                       size_t,
                                       off_t>(&LocalNode::read_,
                                              obj.id,
                                              buf,
                                              size,
                                              off);
    }

    // XXX: move the tracepoint into the previous continuation?
    return rfuture.then(yt::InlineExecutor::get(),
                        [id = obj.id,
                         off,
                         size](ReadFuture r) -> size_t
                        {
                            tracepoint(openvstorage_filesystem,
                                       local_object_read_end,
                                       id.str().c_str(),
                                       off,
                                       size,
                                       r.has_exception());
                            return r.get();
                        });
}

std::pair<FastPathCookie, ClusterNode::ReadFuture>
LocalNode::read(const FastPathCookie& cookie,
                const ObjectId& id,
                uint8_t* buf,
                size_t size,
                off_t off)
{
    tracepoint(openvstorage_filesystem,
	       local_object_read_start,
	       id.str().c_str(),
	       off,
	       size);

    auto on_exc(yt::make_scope_exit_on_exception([&]
                                                 {
                                                     tracepoint(openvstorage_filesystem,
                                                                local_object_read_end,
                                                                id.str().c_str(),
                                                                off,
                                                                size,
                                                                true);
                                                 }));

    RWLockPtr l(get_lock_(id));
    fungi::ScopedReadLock rg(*l);

    vd::SharedVolumePtr vol(cookie->volume.lock());

    if (vol != nullptr)
    {
        ReadFuture rf(read_(vol,
                            buf,
                            size,
                            off).then(yt::InlineExecutor::get(),
                                      [id,
                                       off,
                                       size](ReadFuture r) -> size_t
                                      {
                                          tracepoint(openvstorage_filesystem,
                                                     local_object_read_end,
                                                     id.str().c_str(),
                                                     off,
                                                     size,
                                                     r.has_exception());
                                          return r.get();
                                      }));

        return std::make_pair(std::move(cookie),
                              std::move(rf));
    }
    else
    {
        throw ObjectNotRunningHereException("Outdated cookie, object not running here anymore",
                                            id.str().c_str());
    }
}

ClusterNode::ReadFuture
LocalNode::read_(vd::WeakVolumePtr vol,
                 uint8_t* buf,
                 size_t size,
                 const off_t off)
{
    size_t to_read = 0;
    const uint64_t volume_size = api::GetSize(vol);
    if (off < static_cast<decltype(off)>(volume_size))
    {
        to_read = std::min(volume_size - off,
                           size);

        using Future = boost::future<void>;
        using Reader = Future (*)(vd::WeakVolumePtr,
                                  const uint64_t,
                                  uint8_t*,
                                  uint64_t);

        return maybe_retry_<Future>(static_cast<Reader>(&api::async_read),
                                    vol,
                                    static_cast<const uint64_t>(off),
                                    buf,
                                    to_read)
            .then(yt::InlineExecutor::get(),
                  [to_read](Future f) -> size_t
                  {
                      f.get();
                      return to_read;
                  });
    }
    else
    {
        return boost::make_ready_future(to_read);
    }
}

ClusterNode::WriteFuture
LocalNode::write(const Object& obj,
                 const uint8_t* buf,
                 size_t size,
                 off_t off)
{
    ASSERT(size);

    tracepoint(openvstorage_filesystem,
	       local_object_write_start,
	       obj.id.str().c_str(),
	       off,
	       size);

    auto on_exc(yt::make_scope_exit_on_exception([&]
                                                 {
                                                     tracepoint(openvstorage_filesystem,
                                                                local_object_write_end,
                                                                obj.id.str().c_str(),
                                                                off,
                                                                size,
                                                                true);
                                                 }));

    WriteFuture wfuture;

    RWLockPtr l(get_lock_(obj.id));

    if (is_file(obj))
    {
        fungi::ScopedReadLock rg(*l);
        size = convert_fdriver_exceptions_<size_t,
                                            off_t,
                                            const void*,
                                            size_t>(&fd::ContainerManager::write,
                                                    obj,
                                                    off,
                                                    buf,
                                                    size);
        wfuture = boost::make_ready_future(std::make_pair(size,
                                                          vd::DtlInSync::T));
    }
    else
    {
        fungi::ScopedReadLock rg(*l);
        wfuture = with_volume_pointer_<WriteFuture,
                                       const uint8_t*,
                                       size_t,
                                       off_t>(&LocalNode::write_,
                                              obj.id,
                                              buf,
                                              size,
                                              off);
    }

    // XXX: move the tracepoint into the previous continuation?
    return wfuture.then(yt::InlineExecutor::get(),
                        [id = obj.id,
                         off,
                         size](WriteFuture f) -> WriteResult
                        {
                            tracepoint(openvstorage_filesystem,
                                       local_object_write_end,
                                       id.str().c_str(),
                                       off,
                                       size,
                                       f.has_exception());
                            return f.get();
                        });
}

std::pair<FastPathCookie, ClusterNode::WriteFuture>
LocalNode::write(const FastPathCookie& cookie,
                 const ObjectId& id,
                 const uint8_t* buf,
                 size_t size,
                 off_t off)
{
    ASSERT(size);

    tracepoint(openvstorage_filesystem,
	       local_object_write_start,
	       id.str().c_str(),
	       off,
	       size);

    auto on_exc(yt::make_scope_exit_on_exception([&]
                                                 {
                                                     tracepoint(openvstorage_filesystem,
                                                                local_object_write_end,
                                                                id.str().c_str(),
                                                                off,
                                                                size,
                                                                true);
                                                 }));

    WriteFuture wf;
    FastPathCookie ck;

    RWLockPtr l(get_lock_(id));
    fungi::ScopedReadLock rg(*l);
    std::tie(ck, wf) = write_(cookie,
                              id,
                              buf,
                              size,
                              off);

    return std::make_pair(ck,
                          wf.then(yt::InlineExecutor::get(),
                                  [id,
                                   off,
                                   size](WriteFuture f) -> WriteResult
                                  {
                                      tracepoint(openvstorage_filesystem,
                                                 local_object_write_end,
                                                 id.str().c_str(),
                                                 off,
                                                 size,
                                                 f.has_exception());
                                      return f.get();
                                  }));
}

std::pair<FastPathCookie, ClusterNode::WriteFuture>
LocalNode::write_(const FastPathCookie& cookie,
                  const ObjectId& id,
                  const uint8_t* buf,
                  size_t size,
                  off_t off)
{
    vd::SharedVolumePtr vol(cookie->volume.lock());
    if (vol != nullptr)
    {
        return std::make_pair(cookie,
                              write_(vol,
                                     buf,
                                     size,
                                     off));
    }
    else
    {
        throw ObjectNotRunningHereException("Outdated cookie, object not running here anymore",
                                            id.str().c_str());
    }
}

ClusterNode::WriteFuture
LocalNode::write_(vd::WeakVolumePtr vol,
                  const uint8_t* buf,
                  const size_t size,
                  const off_t off)
{
    using Future = boost::future<vd::DtlInSync>;
    using Writer = Future (*)(vd::WeakVolumePtr,
                               const uint64_t,
                               const uint8_t*,
                               uint64_t);

    Future df(maybe_retry_<Future>(static_cast<Writer>(&api::async_write),
                                   vol,
                                   static_cast<const uint64_t>(off),
                                   buf,
                                   size));

    return df.then(yt::InlineExecutor::get(),
                   [size](Future f) -> WriteResult
                   {
                       return std::make_pair(size, f.get());
                   });
}

ClusterNode::SyncFuture
LocalNode::sync(const Object& obj)
{
    tracepoint(openvstorage_filesystem,
	       local_object_sync_start,
	       obj.id.str().c_str());

    auto on_exception(yt::make_scope_exit_on_exception([&]
                                                       {
                                                           tracepoint(openvstorage_filesystem,
                                                                      local_object_sync_end,
                                                                      obj.id.str().c_str(),
                                                                      1);
                                                       }));

    SyncFuture dtl_in_sync;

    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);

    if (is_file(obj))
    {
        convert_fdriver_exceptions_<void>(&fd::ContainerManager::sync,
                                          obj);
        dtl_in_sync = boost::make_ready_future(vd::DtlInSync::T);
    }
    else
    {
        dtl_in_sync = with_volume_pointer_<SyncFuture>(&LocalNode::sync_,
                                                       obj.id);
    }

    return dtl_in_sync.then(yt::InlineExecutor::get(),
                            [id = obj.id](SyncFuture sf) -> vd::DtlInSync
                            {
                                tracepoint(openvstorage_filesystem,
                                           local_object_sync_end,
                                           id.str().c_str(),
                                           sf.has_exception());
                                return sf.get();
                            });
}

std::pair<FastPathCookie, ClusterNode::SyncFuture>
LocalNode::sync(const FastPathCookie& cookie,
                const ObjectId& id)
{
    tracepoint(openvstorage_filesystem,
	       local_object_sync_start,
	       id.str().c_str());

    auto on_exc(yt::make_scope_exit_on_exception([&]
                                                 {
                                                     tracepoint(openvstorage_filesystem,
                                                                local_object_sync_end,
                                                                id.str().c_str(),
                                                                true);
                                                 }));

    RWLockPtr l(get_lock_(id));
    fungi::ScopedReadLock rg(*l);

    vd::SharedVolumePtr vol(cookie->volume.lock());
    if (vol != nullptr)
    {
        SyncFuture sf(api::async_flush(vol));
        return std::make_pair(cookie,
                              sf.then(yt::InlineExecutor::get(),
                                      [id](SyncFuture f) -> vd::DtlInSync
                                      {
                                          tracepoint(openvstorage_filesystem,
                                                     local_object_sync_end,
                                                     id.str().c_str(),
                                                     f.has_exception());
                                          return f.get();
                                      }));
    }
    else
    {
        throw ObjectNotRunningHereException("Outdated cookie, object not running here anymore",
                                            id.str().c_str());
    }
}

ClusterNode::SyncFuture
LocalNode::sync_(vd::WeakVolumePtr vol)
{
    return api::async_flush(vol);
}

uint64_t
LocalNode::get_size(const Object& obj)
{
    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);

    if (is_file(obj))
    {
        return convert_fdriver_exceptions_<uint64_t>(&fd::ContainerManager::size,
                                                     obj);
    }
    else
    {
        return with_volume_pointer_(&LocalNode::get_size_,
                                    obj.id);
    }
}

uint64_t
LocalNode::get_size_(vd::WeakVolumePtr vol)
{
    return api::GetSize(vol);
}

vd::ClusterMultiplier
LocalNode::get_cluster_multiplier(const Object& obj)
{
    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);
    return with_volume_pointer_(&LocalNode::get_cluster_multiplier_,
                                obj.id);
}

vd::ClusterMultiplier
LocalNode::get_cluster_multiplier_(vd::WeakVolumePtr vol)
{
    return api::GetClusterMultiplier(vol);
}

vd::CloneNamespaceMap
LocalNode::get_clone_namespace_map(const Object& obj)
{
    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);
    return with_volume_pointer_(&LocalNode::get_clone_namespace_map_,
                                obj.id);
}

vd::CloneNamespaceMap
LocalNode::get_clone_namespace_map_(vd::WeakVolumePtr vol)
{
    return api::GetCloneNamespaceMap(vol);
}

std::vector<vd::ClusterLocation>
LocalNode::get_page(const Object& obj,
                    const vd::ClusterAddress ca)
{
    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);
    return with_volume_pointer_(&LocalNode::get_page_,
                                obj.id,
                                ca);
}

std::vector<vd::ClusterLocation>
LocalNode::get_page_(vd::WeakVolumePtr vol,
                     const vd::ClusterAddress ca)
{
    return api::GetPage(vol, ca);
}

void
LocalNode::resize(const Object& obj,
                  uint64_t newsize)
{
    RWLockPtr l(get_lock_(obj.id));
    fungi::ScopedReadLock rg(*l);

    if (is_file(obj))
    {
        convert_fdriver_exceptions_<void,
                                    uint64_t>(&fd::ContainerManager::resize,
                                              obj,
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
LocalNode::resize_(vd::WeakVolumePtr vol,
                   uint64_t newsize)
{
    LOCKVD();

    const uint64_t csize = api::GetClusterSize(vol);
    uint64_t clusters = newsize / csize;
    if (newsize % csize)
    {
        LOG_WARN("new size " << newsize << " not aligned to cluster size " <<
                 csize << " - trying best effort");
        ++clusters;
    }

    api::Resize(vol,
                clusters);
}

namespace
{

LocalNode::Deadline
get_deadline(const boost::optional<bc::milliseconds>& maybe_timeout_ms)
{
    if (maybe_timeout_ms)
    {
        return LocalNode::Clock::now() + *maybe_timeout_ms;
    }
    else
    {
        return LocalNode::Clock::time_point::max();
    }
}

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
        convert_fdriver_exceptions_<void>(&fd::ContainerManager::unlink,
                                          obj);
    }
    else
    {
        with_volume_pointer_(&LocalNode::destroy_,
                             obj.id,
                             vd::DeleteLocalData::T,
                             vd::RemoveVolumeCompletely::T,
                             get_deadline(boost::none));
    }
}

void
LocalNode::destroy_(vd::WeakVolumePtr vol,
                    vd::DeleteLocalData delete_local,
                    vd::RemoveVolumeCompletely remove_completely,
                    Deadline deadline)
{
    const vd::VolumeId id(api::getVolumeId(vol));

    LOG_TRACE(id << ": delete_local " << delete_local <<
              ", remove_completely: " << remove_completely);

    get_lock_(static_cast<const ObjectId>(id))->assertWriteLocked();

    if (remove_completely == vd::RemoveVolumeCompletely::F and
        delete_local == vd::DeleteLocalData::T)
    {
        {
            LOCKVD();
            LOG_INFO(vd::SharedVolumePtr(vol)->getName() << ": trying to sync to the backend");
            api::scheduleBackendSync(id);
        }

        const Clock::duration sleep_msecs =
            bc::milliseconds(vrouter_backend_sync_check_interval_ms.value());

        while (true)
        {
            if (Clock::now() >= deadline)
            {
                LOG_ERROR(id <<
                          ": timeout (deadline: " << deadline << ") syncing to the backend");
                throw SyncTimeoutException("Timeout syncing volume to backend",
                                           id.str().c_str());
            }

            {
                LOCKVD();
                const bool synced = api::isVolumeSynced(id);
                if (synced)
                {
                    LOG_INFO(id << ": synced to the backend");
                    break;
                }
            }

            auto now = Clock::now();
            if (now < deadline)
            {
                const Clock::duration diff = bc::duration_cast<bc::milliseconds>(deadline - now);
                boost::this_thread::sleep_for(std::min(sleep_msecs,
                                                       diff));
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
        convert_fdriver_exceptions_<void>(&fd::ContainerManager::drop_from_cache,
                                          obj);
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
                               vd::IgnoreFOCIfUnreachable::F);
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
        convert_fdriver_exceptions_<void>(&fd::ContainerManager::restart,
                                          reg.object());
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
                                 vd::IgnoreFOCIfUnreachable::F);
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
        convert_fdriver_exceptions_<void>(&fd::ContainerManager::restart,
                                          obj);
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
        convert_fdriver_exceptions_<void>(&fd::ContainerManager::create,
                                          obj);
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
            auto params(vd::VanillaVolumeConfigParameters(static_cast<const vd::VolumeId>(id),
                                                          nspace,
                                                          vd::VolumeSize(0),
                                                          reg->owner_tag)
                        .sco_multiplier(vd::SCOMultiplier(vrouter_sco_multiplier.value()))
                        .metadata_backend_config(std::move(mdb_config)));

            const size_t lba_size(vd::VolumeConfig::default_lba_size());
            const size_t cluster_size(api::getDefaultClusterSize());

            THROW_UNLESS(cluster_size >= lba_size);
            THROW_UNLESS((cluster_size % lba_size) == 0);

            vd::ClusterMultiplier cmult(cluster_size / lba_size);
            params.cluster_multiplier(cmult);

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
                         get_deadline(boost::none));
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

    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);

    bool synced = false;
    while (std::chrono::steady_clock::now() < end)
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
        vrouter_.delete_snapshot_local(src_id, snap);
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
        vrouter_.delete_snapshot_local(src_id,
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

    // XXX: use a bigger buffer
    std::vector<uint8_t> buf(4096);
    off_t offset = 0;

    while (bytes_to_copy > 0)
    {
        const size_t read_size = read(src_obj,
                                      buf.data(),
                                      buf.size(),
                                      offset).get();

        const size_t write_size = write(dst_obj,
                                        buf.data(),
                                        read_size,
                                        offset).get().first;
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
LocalNode::create_snapshot(const ObjectId& id,
                           const vd::SnapshotName& snap_id,
                           const int64_t timeout)
{
    vd::SnapshotName snapname;
    const vd::VolumeId volid(id);
    {
        fungi::ScopedLock l(api::getManagementMutex());
        snapname = api::createSnapshot(volid,
                                       vd::SnapshotMetaData(),
                                       &snap_id);
    }

    bool synced = false;
    if (timeout > 0)
    {
        auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
        while (std::chrono::steady_clock::now() < end)
        {
            {
                fungi::ScopedLock l(api::getManagementMutex());
                synced = api::isVolumeSyncedUpTo(volid,
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
    }
    else if (timeout == 0)
    {
        while (not synced)
        {
            {
                fungi::ScopedLock l(api::getManagementMutex());
                synced = api::isVolumeSyncedUpTo(volid,
                                                 snapname);
            }
            if (synced)
            {
                break;
            }
        }
    }
    else
    {
        return;
    }

    if (not synced)
    {
        vrouter_.delete_snapshot_local(id, snapname);
        throw SyncTimeoutException("timeout syncing snapshot to backend\n");
    }
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

std::list<vd::SnapshotName>
LocalNode::list_snapshots(const ObjectId& id)
{
    std::list<vd::SnapshotName> snaps;
    with_api_exception_conversion([&]
    {
        const vd::VolumeId vid(static_cast<const vd::VolumeId>(id));
        api::showSnapshots(vid,
                           snaps);
    });
    return snaps;
}

bool
LocalNode::is_volume_synced_up_to(const ObjectId& id,
                                  const vd::SnapshotName& snapname)
{
    LOCKVD();
    const vd::VolumeId vid(static_cast<const vd::VolumeId>(id));
    return api::isVolumeSyncedUpTo(vid,
                                   snapname);
}

std::vector<scrubbing::ScrubWork>
LocalNode::get_scrub_work(const ObjectId& id,
                          const boost::optional<vd::SnapshotName>& start_snap,
                          const boost::optional<vd::SnapshotName>& end_snap)
{
    LOG_INFO(id << ": getting scrub work, start snapshot " << start_snap <<
             ", end snapshot " << end_snap);

    std::vector<scrubbing::ScrubWork> work;

    {
        // not strictly necessary?
        RWLockPtr l(get_lock_(id));
        fungi::ScopedReadLock rg(*l);

        ObjectRegistrationPtr reg(vrouter_.object_registry()->find_throw(id,
                                                                         IgnoreCache::T));
        if (reg->node_id != vrouter_.node_id())
        {
            LOG_WARN(id << ": not running here (" << vrouter_.node_id() << ") but on " <<
                     reg->node_id);
            throw ObjectNotRunningHereException("object not running here",
                                                id.str().c_str());
        }

        if (reg->treeconfig.object_type != ObjectType::Volume)
        {
            LOG_ERROR(id << ": refusing to get scrub work - invalid object type " <<
                      reg->treeconfig.object_type);
            throw InvalidOperationException() <<
                error_object_id(id) <<
                error_desc("invalid object type");
        }

        auto fun([&]
                 {
                     const auto& vid = static_cast<const vd::VolumeId&>(id.str());
                     work = api::getScrubbingWork(vid,
                                                  start_snap,
                                                  end_snap);
                 });

        with_api_exception_conversion(std::move(fun));
    }

    const ScrubManager::ParentScrubs
        pending_scrubs(scrub_manager_->get_parent_scrubs());

    std::vector<scrubbing::ScrubWork> res;

    if (pending_scrubs.size() >= scrub_manager_max_parent_scrubs.value())
    {
        LOG_WARN(id << ": not handing out further scrub work - there are already " <<
                 pending_scrubs.size() << " scrub replies queued up (max: " <<
                 scrub_manager_max_parent_scrubs.value() << ")");
    }
    else
    {
        res.reserve(work.size());

        // Sub-awesome time complexity. Do we care?
        for (auto&& w : work)
        {
            bool pending = false;
            for (const auto& p : pending_scrubs)
            {
                if (p.second == id and
                    p.first.ns_ == w.ns_ and
                    p.first.snapshot_name_ == w.snapshot_name_)
                {
                    pending = true;
                    break;
                }
            }

            if (not pending)
            {
                res.push_back(std::move(w));
            }
        }
    }

    return res;
}

void
LocalNode::queue_scrub_reply(const ObjectId& oid,
                             const scrubbing::ScrubReply& reply)
{
    scrub_manager_->queue_scrub_reply(oid,
                                      reply);
}

boost::optional<be::Garbage>
LocalNode::apply_scrub_reply_(const ObjectId& id,
                              const scrubbing::ScrubReply& rsp,
                              const vd::ScrubbingCleanup cleanup)
{
    LOG_INFO(id << ": applying scrub result");

    const vd::VolumeId& vid = static_cast<const vd::VolumeId&>(id.str());
    boost::optional<be::Garbage> maybe_garbage;
    vd::ApplyRelocsContinuations conts;

    vd::TLogId tlog_id;

    {
        // not strictly necessary?
        RWLockPtr l(get_lock_(id));
        fungi::ScopedReadLock rg(*l);

        auto fun([&]
                 {
                     std::tie(maybe_garbage, conts) = api::applyScrubbingWork(vid,
                                                                              rsp,
                                                                              cleanup);
                     tlog_id = api::scheduleBackendSync(vid);
                 });

        with_api_exception_conversion(std::move(fun));
    }

    yt::SteadyTimer timer;

    while (true)
    {
        bool nsync = false;

        {
            // not strictly necessary?
            RWLockPtr l(get_lock_(id));
            fungi::ScopedReadLock rg(*l);

            try
            {
                LOCKVD();
                nsync = api::isVolumeSyncedUpTo(vid,
                                                tlog_id);
            }
            catch (vd::VolManager::VolumeDoesNotExistException&)
            {
                LOG_INFO(vid << ": volume does not exist (anymore?) - assuming it's gone for good or it was moved to another node, either of which is fine.");
                nsync = true;
            }
        }

        if (nsync)
        {
            break;
        }
        else if (timer.elapsed() > bc::seconds(scrub_manager_sync_wait_secs.value()))
        {
            LOG_ERROR(id << ": applying " << rsp << " timed out");
            throw fungi::IOException("scrub reply application timed out",
                                     id.str().c_str());
        }
        else
        {
            LOG_INFO(vid <<
                     ": not sync'ed to backend yet - going to sleep for a while");
            boost::this_thread::sleep_for(bc::seconds(1));
        }
    }

    std::vector<std::future<void>> futs;
    futs.reserve(conts.size());
    const std::chrono::seconds timeout(scrub_manager_sync_wait_secs.value());

    for (size_t i = 0; i < conts.size(); ++i)
    {
        futs.push_back(std::async(std::launch::async,
                                  [&conts, i, timeout]
                                  {
                                      conts[i](timeout);
                                  }));
    }

    for (auto& f : futs)
    {
        f.get();
    }

    return maybe_garbage;
}

void
LocalNode::collect_scrub_garbage_(be::Garbage garbage)
{
    api::backend_garbage_collector()->queue(std::move(garbage));
}

void
LocalNode::transfer(const Object& obj,
                    const NodeId target_node,
                    MaybeSyncTimeoutMilliSeconds maybe_sync_timeout_ms)
{
    LOG_INFO(obj << ": target_node " << target_node);

    const Deadline deadline(get_deadline(maybe_sync_timeout_ms));

    RWLockPtr l(get_lock_(obj.id));
    boost::unique_lock<std::remove_reference<decltype(*l)>::type> u(*l, deadline);
    if (!u)
    {
        LOG_ERROR(obj << ": deadline " << deadline <<
                  " exceeded (timeout: " << maybe_sync_timeout_ms);
        throw SyncTimeoutException("Timeout syncing object to backend",
                                   obj.id.str().c_str());
    }

    if (is_file(obj))
    {
        convert_fdriver_exceptions_<void>(&fd::ContainerManager::drop_from_cache,
                                          obj);
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
                             deadline);
    }

    ObjectRegistrationPtr
        reg(vrouter_.object_registry()->find_throw(obj.id,
                                                   IgnoreCache::T));
    vrouter_.object_registry()->migrate(obj.id,
                                        vrouter_.node_id(),
                                        target_node);

    vrouter_.event_publisher()->publish(FileSystemEvents::owner_changed(obj.id,
                                                                        target_node));
}

void
LocalNode::adjust_failovercache_config_(const ObjectId& id,
                                        const FailOverCacheConfigMode& mode,
                                        const MaybeFailOverCacheConfig& config)
{
    LOG_INFO(id);

    const vd::VolumeId& vid = static_cast<const vd::VolumeId>(id);

    do_adjust_failovercache_config_(vid,
                                    config);

    vrouter_.object_registry()->set_foc_config_mode(id,
                                                    mode);
}

void
LocalNode::do_adjust_failovercache_config_(const vd::VolumeId& vid,
                                           const MaybeFailOverCacheConfig& config)
{
    ASSERT_LOCKABLE_LOCKED(api::getManagementMutex());

    const boost::optional<vd::FailOverCacheConfig>
        old_config(api::getFailOverCacheConfig(vid));

    if (old_config != config)
    {
        LOG_INFO(vid << ": setting FailOverCacheConfig from " <<
                 old_config << " -> " << config);
        try
        {
            api::setFailOverCacheConfig(vid,
                                        config);
        }
        CATCH_STD_ALL_LOG_IGNORE(vid <<
                                 ": error while setting FailOverCacheConfig to " <<
                                 config);
    }
    else
    {
        LOG_INFO(vid << ": FailOverCacheConfig is unchanged: " << old_config);
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
                         get_deadline(boost::none));
}

void
LocalNode::stop(const Object& obj,
                vd::DeleteLocalData delete_local_data)
{
    LOG_INFO("Stopping " << obj);

    if (is_file(obj))
    {
        convert_fdriver_exceptions_<void>(&fd::ContainerManager::drop_from_cache,
                                          obj);
    }
    else
    {
        stop_volume_(obj.id,
                     delete_local_data);
    }
}

uint64_t
LocalNode::volume_potential(const boost::optional<vd::ClusterSize>& c,
                            const boost::optional<vd::SCOMultiplier>& s,
                            const boost::optional<vd::TLogMultiplier>& t)
{
    return api::volumePotential(c ?
                                *c :
                                api::getDefaultClusterSize(),
                                s ?
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
                                 const MaybeFailOverCacheConfig& cfg)
{
    adjust_failovercache_config_(oid,
                                 FailOverCacheConfigMode::Manual,
                                 cfg);
}

void
LocalNode::set_manual_default_foc_config(const MaybeFailOverCacheConfig& cfg)
{
    ASSERT_LOCKABLE_LOCKED(api::getManagementMutex());

    std::list<vd::VolumeId> vols;
    api::getVolumeList(vols);

    for (const auto& vid : vols)
    {
        try
        {
            const FailOverCacheConfigMode mode = get_foc_config_mode(ObjectId(vid.str()));
            if (mode == FailOverCacheConfigMode::Automatic)
            {
                do_adjust_failovercache_config_(vid,
                                                cfg);
            }
        }
        CATCH_STD_ALL_LOG_IGNORE(vid << ": failed to adjust FailOverCacheConfig");
    }
}

void
LocalNode::set_automatic_foc_config(const ObjectId& oid)
{
    adjust_failovercache_config_(oid,
                                 FailOverCacheConfigMode::Automatic,
                                 vrouter_.failoverconfig_as_it_should_be());
}

LocalNode::MaybeFailOverCacheConfig
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

ScrubManager&
LocalNode::scrub_manager()
{
    return *scrub_manager_;
}

FastPathCookie
LocalNode::fast_path_cookie(const Object& obj)
{
    if (is_file(obj))
    {
        return nullptr;
    }
    else
    {
        LOCKVD();
        boost::optional<vd::WeakVolumePtr>
            maybe_vol(api::get_volume_pointer_no_throw(static_cast<vd::VolumeId>(obj.id)));
        if (maybe_vol)
        {
            return std::make_shared<LocalVolumeCookie>(*maybe_vol);
        }
        else
        {
            return nullptr;
        }
    }
}

}
