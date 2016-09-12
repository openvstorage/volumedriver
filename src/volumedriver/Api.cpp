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

#include "Api.h"
#include "MetaDataStoreInterface.h"
#include "ScrubWork.h"
#include "SnapshotManagement.h"
#include "TransientException.h"
#include "VolManager.h"

#include <youtils/BuildInfoString.h>
#include <youtils/Logger.h>
#include "failovercache/fungilib/Mutex.h"

using namespace volumedriver;

namespace be = backend;
namespace bpt = boost::property_tree;

// this one also comes in via Types.h - clean this up.
// namespace fs = boost::filesystem;
namespace vd = volumedriver;

namespace
{
DECLARE_LOGGER("Api");
}

fungi::Mutex&
api::getManagementMutex()
{
    return VolManager::get()->mgmtMutex_;
}

void
api::createNewVolume(const vd::VanillaVolumeConfigParameters& params,
                     const CreateNamespace create_namespace)
{
    VolManager::get()->createNewVolume(params,
                                       create_namespace);
}

WriteOnlyVolume*
api::createNewWriteOnlyVolume(const vd::WriteOnlyVolumeConfigParameters& params)
{
    return VolManager::get()->createNewWriteOnlyVolume(params);
}

void
api::createClone(const vd::CloneVolumeConfigParameters& params,
                 const vd::PrefetchVolumeData prefetch,
                 vd::CreateNamespace create_namespace)
{
    VolManager::get()->createClone(params,
                                   prefetch,
                                   create_namespace);
}

void
api::updateMetaDataBackendConfig(vd::WeakVolumePtr vol,
                                 const vd::MetaDataBackendConfig& mdb)
{
    SharedVolumePtr(vol)->updateMetaDataBackendConfig(mdb);
}

void
api::updateMetaDataBackendConfig(const vd::VolumeId& volume_id,
                                 const vd::MetaDataBackendConfig& mdb)
{
    auto vol = VolManager::get()->findVolume_(volume_id);
    updateMetaDataBackendConfig(vol,
                                mdb);
}

void
api::Write(WeakVolumePtr vol,
           const uint64_t lba,
           const uint8_t *buf,
           const uint64_t buflen)
{
    SharedVolumePtr(vol)->write(lba,
                                buf,
                                buflen);
}

void
api::Read(WeakVolumePtr vol,
          const uint64_t lba,
          uint8_t *buf,
          const uint64_t buflen)
{
    SharedVolumePtr(vol)->read(lba,
                               buf,
                               buflen);
}

void
api::Write(WriteOnlyVolume* vol,
           uint64_t lba,
           const uint8_t *buf,
           uint64_t buflen)
{
    VERIFY(vol);
    vol->write(lba,
               buf,
               buflen);
}

void
api::Sync(vd::WeakVolumePtr vol)
{
    SharedVolumePtr(vol)->sync();
}

uint64_t
api::GetSize(vd::WeakVolumePtr vol)
{
    return SharedVolumePtr(vol)->getSize();
}

void
api::Resize(vd::WeakVolumePtr vol,
            uint64_t clusters)
{
    SharedVolumePtr(vol)->resize(clusters);
}

uint64_t
api::GetLbaSize(vd::WeakVolumePtr vol)
{
    return SharedVolumePtr(vol)->getLBASize();
}

uint64_t
api::GetClusterSize(vd::WeakVolumePtr vol)
{
    return SharedVolumePtr(vol)->getClusterSize();
}

void
api::Init(const fs::path& path,
          events::PublisherPtr event_publisher)
{
    vd::VolManager::start(path,
                          event_publisher);
}

void
api::Init(const bpt::ptree& pt,
          events::PublisherPtr event_publisher)
{
    vd::VolManager::start(pt,
                          event_publisher);
}

void
api::Exit(void)
{
    vd::VolManager::stop();
}

vd::WeakVolumePtr
api::getVolumePointer(const VolumeId& volname)
{
    return VolManager::get()->findVolume_(volname);
}

boost::optional<vd::WeakVolumePtr>
api::get_volume_pointer_no_throw(const VolumeId& volname)
{
    SharedVolumePtr v(VolManager::get()->findVolume_noThrow_(volname));
    if (not v)
    {
        return boost::none;
    }
    else
    {
        WeakVolumePtr w(v);
        return w;
    }
}

vd::WeakVolumePtr
api::getVolumePointer(const Namespace& ns)
{
    return VolManager::get()->findVolume_(ns);
}

const vd::VolumeId
api::getVolumeId(vd::WeakVolumePtr vol)
{
    return SharedVolumePtr(vol)->getName();
}

bool
api::volumeExists(const VolumeId& volname)
{
    return VolManager::get()->findVolume_noThrow_(volname) != nullptr;
}

void
api::getVolumeList(std::list<VolumeId>& l)
{
    VolManager::get()->getVolumeList(l);
}

void
api::getVolumesOverview(std::map<vd::VolumeId, vd::VolumeOverview>& out_map)
{
    VolManager::get()->getVolumesOverview(out_map);
}

VolumeConfig
api::getVolumeConfig(const vd::VolumeId& volName)
{
    const SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->get_config();
}

void
api::local_restart(const Namespace& ns,
                   const vd::OwnerTag owner_tag,
                   const vd::FallBackToBackendRestart fallback,
                   const vd::IgnoreFOCIfUnreachable ignoreFOCIfUnreachable)
{
    VolManager::get()->local_restart(ns,
                                     owner_tag,
                                     fallback,
                                     ignoreFOCIfUnreachable);
}

void
api::backend_restart(const Namespace& ns,
                     const vd::OwnerTag owner_tag,
                     const vd::PrefetchVolumeData prefetch,
                     const vd::IgnoreFOCIfUnreachable ignoreFOCIfUnreachable)
{
    VolManager::get()->backend_restart(ns,
                                       owner_tag,
                                       prefetch,
                                       ignoreFOCIfUnreachable);
}

WriteOnlyVolume*
api::restartVolumeWriteOnly(const Namespace& ns,
                            const vd::OwnerTag owner_tag)
{
    return VolManager::get()->restartWriteOnlyVolume(ns,
                                                     owner_tag);
}

void
api::destroyVolume(const vd::VolumeId& volName,
                   const vd::DeleteLocalData delete_local_data,
                   const vd::RemoveVolumeCompletely remove_volume_completely,
                   const vd::DeleteVolumeNamespace delete_volume_namespace,
                   const vd::ForceVolumeDeletion force_volume_deletion)
{
    VolManager::get()->destroyVolume(volName,
                                     delete_local_data,
                                     remove_volume_completely,
                                     delete_volume_namespace,
                                     force_volume_deletion);
}

void
api::destroyVolume(vd::WeakVolumePtr vol,
                   const vd::DeleteLocalData delete_local_data,
                   const vd::RemoveVolumeCompletely remove_volume_completely,
                   const vd::DeleteVolumeNamespace delete_volume_namespace,
                   const vd::ForceVolumeDeletion force_volume_deletion)

{
    VolManager::get()->destroyVolume(SharedVolumePtr(vol),
                                     delete_local_data,
                                     remove_volume_completely,
                                     delete_volume_namespace,
                                     force_volume_deletion);
}

void
api::destroyVolume(WriteOnlyVolume* vol,
                   const RemoveVolumeCompletely remove_volume_completely)
{
    VolManager::get()->destroyVolume(vol,
                                     remove_volume_completely);
}

void
api::removeLocalVolumeData(const be::Namespace& nspace)
{
    VolManager::get()->removeLocalVolumeData(nspace);
}

SnapshotName
api::createSnapshot(vd::WeakVolumePtr wv,
                    const vd::SnapshotMetaData& metadata,
                    const vd::SnapshotName* const snapid,
                    const UUID& uuid)
{
    SharedVolumePtr v(wv);

    vd::SnapshotName snapname;

    if (snapid == nullptr or
        snapid->empty())
    {
        // boost certainly has a nice wrapper around that?
        struct timeval tv;
        if (gettimeofday(&tv, 0) == -1)
        {
            throw fungi::IOException(v->getName().c_str(),
                                     "failed to get timestamp for snapshot");
        }

        time_t timeT = tv.tv_sec;
        struct tm* tm = localtime(&timeT);
        char s[1024];

        if (strftime(s, 1024, "%a %b %Y %T ", tm) == 0)
        {
            throw fungi::IOException(v->getName().c_str(),
                                     "failed to get timestamp for snapshot");
        }
        snapname = vd::SnapshotName(std::string(s) + UUID().str());
    }
    else
    {
        snapname = *snapid;
    }

    v->createSnapshot(snapname,
                      metadata,
                      uuid);
    return snapname;
}


vd::SnapshotName
api::createSnapshot(vd::WriteOnlyVolume* v,
                    const vd::SnapshotMetaData& metadata,
                    const vd::SnapshotName* const snapid,
                    const UUID& uuid)
{
    VERIFY(v);

    SnapshotName snapname;

    if (snapid == nullptr or
        snapid->empty())
    {
        // boost certainly has a nice wrapper around that?
        struct timeval tv;
        if (gettimeofday(&tv, 0) == -1)
        {
            throw fungi::IOException(v->getName().c_str(),
                                     "failed to get timestamp for snapshot");
        }

        time_t timeT = tv.tv_sec;
        struct tm* tm = localtime(&timeT);
        char s[1024];

        if (strftime(s, 1024, "%a %b %Y %T ", tm) == 0)
        {
            throw fungi::IOException(v->getName().c_str(),
                                     "failed to get timestamp for snapshot");
        }
        snapname = SnapshotName(std::string(s) + UUID().str());
    }
    else
    {
        snapname = *snapid;
    }

    v->createSnapshot(snapname,
                      metadata,
                      uuid);
    return snapname;
}

Snapshot
api::getSnapshot(const vd::VolumeId& id,
                 const vd::SnapshotName& snapname)
{
    ASSERT_LOCKABLE_LOCKED(getManagementMutex());
    const auto vol = VolManager::get()->findVolume_(id);
    return getSnapshot(vol, snapname);
}

Snapshot
api::getSnapshot(const vd::WeakVolumePtr vol,
                 const vd::SnapshotName& snapname)
{
    return SharedVolumePtr(vol)->getSnapshot(snapname);
}

Snapshot
api::getSnapshot(const vd::WriteOnlyVolume* vol,
                 const vd::SnapshotName& snapname)
{
    VERIFY(vol);
    return vol->getSnapshot(snapname);
}

bool
api::checkSnapshotUUID(const WriteOnlyVolume* vol,
                       const vd::SnapshotName& snapshotName,
                       const vd::UUID& uuid)
{
    VERIFY(vol);
    return vol->checkSnapshotUUID(snapshotName,
                                  uuid);
}

bool
api::snapshotExists(const vd::WriteOnlyVolume* vol,
                    const vd::SnapshotName& snapshotName)
{
    VERIFY(vol);
    return vol->snapshotExists(snapshotName);
}

vd::SnapshotName
api::createSnapshot(const VolumeId& volName,
                    const vd::SnapshotMetaData& metadata,
                    const vd::SnapshotName* const snapid)
{
    auto v = VolManager::get()->findVolume_(volName);
    return createSnapshot(v,
                          metadata,
                          snapid);
}

void
api::setAsTemplate(const VolumeId& volId)
{
    VolManager::get()->setAsTemplate(volId);
}

void
api::showSnapshots(const VolumeId& volName,
                   std::list<vd::SnapshotName>& l)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    v->listSnapshots(l);
}

void
api::showSnapshots(const WriteOnlyVolume* v,
                   std::list<vd::SnapshotName>& l)
{
    VERIFY(v);
    v->listSnapshots(l);
}

void
api::destroySnapshot(const VolumeId& volName,
                     const vd::SnapshotName& snapid)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    v->deleteSnapshot(snapid);
}

void
api::restoreSnapshot(const VolumeId& volName,
                     const vd::SnapshotName& snapid)
{
    VolManager::get()->restoreSnapshot(volName, snapid);
}

void
api::restoreSnapshot(WriteOnlyVolume* vol,
                     const vd::SnapshotName& snapid)
{
    VERIFY(vol);
    vol->restoreSnapshot(snapid);
}

// void
// api::getSnapshotScrubbingWork(const vd::VolumeId& volName,
//                               const boost::optional<std::string>& start_snap,
//                               const boost::optional<std::string>& end_snap,
//                               VolumeWork& work)
// {
//     Volume* v = VolManager::get()->findVolume_(volName);
//     v->getSnapshotScrubbingWork(start_snap, end_snap, work);
// }

uint64_t
api::VolumeDataStoreWriteUsed(const vd::VolumeId& volName)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->VolumeDataStoreWriteUsed();
}

uint64_t
api::VolumeDataStoreReadUsed(const vd::VolumeId& volName)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->VolumeDataStoreReadUsed();
}

PerformanceCounters&
api::performance_counters(const vd::VolumeId& volName)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->performance_counters();
}

bool
api::getHalted(const vd::VolumeId& volName)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->is_halted();
}

uint64_t
api::getCacheHits(const vd::VolumeId& volName)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getCacheHits();
}

uint64_t
api::getCacheMisses(const vd::VolumeId& volName)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getCacheMisses();
}

uint64_t
api::getNonSequentialReads(const vd::VolumeId& volName)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getNonSequentialReads();
}

uint64_t
api::getClusterCacheHits(const vd::VolumeId& volName)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getClusterCacheHits();
}

uint64_t
api::getClusterCacheMisses(const vd::VolumeId& volName)
{
    vd::SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getClusterCacheMisses();
}

uint64_t
api::getQueueCount(const VolumeId& volName)
{
    return VolManager::get()->getQueueCount(volName);
}

uint64_t
api::getQueueSize(const VolumeId& volName)
{
    return VolManager::get()->getQueueSize(volName);
}

uint64_t
api::getTLogUsed(const vd::VolumeId& volName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getTLogUsed();
}

vd::TLogId
api::scheduleBackendSync(const vd::VolumeId& volName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->scheduleBackendSync();
}

bool
api::isVolumeSynced(const vd::VolumeId& volName)
{

    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->isSyncedToBackend();
}

bool
api::isVolumeSyncedUpTo(const vd::VolumeId& volName,
                        const vd::SnapshotName& snapshotName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->isSyncedToBackendUpTo(snapshotName);
}

bool
api::isVolumeSyncedUpTo(const vd::VolumeId& vol_id,
                        const vd::TLogId& tlog_id)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(vol_id);
    return v->isSyncedToBackendUpTo(tlog_id);
}

bool
api::isVolumeSyncedUpTo(const WriteOnlyVolume* v,
                        const vd::SnapshotName& snapshotName)
{
    VERIFY(v);
    return v->isSyncedToBackendUpTo(snapshotName);
}

std::string
api::getRevision()
{
    return youtils::buildInfoString();

}

void
api::removeNamespaceFromSCOCache(const backend::Namespace& nsName)
{
    VolManager::get()->getSCOCache()->removeDisabledNamespace(nsName);
}

void
api::getSCOCacheMountPointsInfo(vd::SCOCacheMountPointsInfo& info)
{
    return VolManager::get()->getSCOCache()->getMountPointsInfo(info);
}

vd::SCOCacheNamespaceInfo
api::getVolumeSCOCacheInfo(const backend::Namespace ns)
{
    return VolManager::get()->getSCOCache()->getNamespaceInfo(ns);
}

vd::VolumeFailOverState
api::getFailOverMode(const vd::VolumeId& volName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getVolumeFailOverState();
}

boost::optional<vd::FailOverCacheConfig>
api::getFailOverCacheConfig(const vd::VolumeId& volName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getFailOverCacheConfig();
}

void
api::setFailOverCacheConfig(const vd::VolumeId& volName,
                            const boost::optional<vd::FailOverCacheConfig>& maybe_foc_config)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    v->setFailOverCacheConfig(maybe_foc_config);
}

uint64_t
api::getCurrentSCOCount(const vd::VolumeId& volName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return  v->getSnapshotSCOCount();
}

uint64_t
api::getSnapshotSCOCount(const vd::VolumeId& volName,
                         const vd::SnapshotName& snapid)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getSnapshotSCOCount(snapid);
}

void
api::setFOCTimeout(const vd::VolumeId& volName,
                   const boost::chrono::seconds timeout)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    v->setFOCTimeout(timeout);
}

bool
api::checkVolumeConsistency(const vd::VolumeId& volName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->checkConsistency();
}

uint64_t
api::getSnapshotBackendSize(const vd::VolumeId& volName,
                            const vd::SnapshotName& snapName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getSnapshotBackendSize(snapName);
}

uint64_t
api::getCurrentBackendSize(const vd::VolumeId& volName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->getCurrentBackendSize();
}


void
api::dumpSCOInfo(const std::string& outFile)
{
    VolManager::get()->getSCOCache()->dumpSCOInfo(outFile);
}


void
api::addSCOCacheMountPoint(const std::string& path,
                           uint64_t size)
{
    MountPointConfig cfg(path, size);
    VolManager::get()->getSCOCache()->addMountPoint(cfg);
}

void
api::removeSCOCacheMountPoint(const std::string& path)
{
    VolManager::get()->getSCOCache()->removeMountPoint(path);
}

void
api::addClusterCacheDevice(const std::string& /*path*/,
                           const uint64_t /*size*/)
{
    //    VolManager::get()->getClusterCache().addDevice(path,
    //    size);
    LOG_WARN( "Not supported in this release");
    throw fungi::IOException("Not Supported in this release");

}

void
api::removeClusterCacheDevice(const std::string& /*path*/)
{
    //    VolManager::get()->getClusterCache().removeDevice(path);;
    LOG_WARN("Not supported in this release");
    throw fungi::IOException("Not Supported in this release");
}

void
api::setClusterCacheDeviceOffline(const std::string& path)
{
    VolManager::get()->getClusterCache().offlineDevice(path);
}

void
api::setClusterCacheDeviceOnline(const std::string& path)
{
    VolManager::get()->getClusterCache().onlineDevice(path);
}

void
api::getClusterCacheDeviceInfo(vd::ClusterCache::ManagerType::Info& info)
{
    VolManager::get()->getClusterCacheDeviceInfo(info);
}

vd::ClusterCacheVolumeInfo
api::getClusterCacheVolumeInfo(const vd::VolumeId& volName)
{
    return VolManager::get()->findVolume_(volName)->getClusterCacheVolumeInfo();
}

vd::ClusterSize
api::getDefaultClusterSize()
{
    return vd::ClusterSize(VolManager::get()->default_cluster_size.value());
}

// void
// api::changeThrottling(unsigned throttle_usecs)
// {
//     VolManager::get()->setThrottleUsecsdatastore_throttle_usecs.value().store(throttle_usecs);
// }

void
api::startPrefetching(const vd::VolumeId& volName)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    v->startPrefetch();
}

void
api::getClusterCacheStats(uint64_t& num_hits,
                          uint64_t& num_misses,
                          uint64_t& num_entries)
{
    VolManager::get()->getClusterCacheStats(num_hits,
                                            num_misses,
                                            num_entries);
}

vd::MetaDataStoreStats
api::getMetaDataStoreStats(const vd::VolumeId& volname)
{
    MetaDataStoreStats mdss;
    vd::MetaDataStoreInterface* mdi =
        VolManager::get()->findVolume_(volname)->getMetaDataStore();
    mdi->getStats(mdss);
    return mdss;
}

boost::variant<youtils::UpdateReport,
               youtils::ConfigurationReport>
api::updateConfiguration(const bpt::ptree& pt)
{
    return VolManager::get()->updateConfiguration(pt);
}

boost::variant<youtils::UpdateReport,
               youtils::ConfigurationReport>
api::updateConfiguration(const fs::path& path)
{
    return VolManager::get()->updateConfiguration(path);
}

void
api::persistConfiguration(const fs::path& path,
                          const bool reportDefault)
{
    VolManager::get()->persistConfiguration(path,
                                            reportDefault ?
                                            ReportDefault::T :
                                            ReportDefault::F);
}

void
api::persistConfiguration(bpt::ptree& pt,
                          const bool reportDefault)
{
    VolManager::get()->persistConfiguration(pt,
                                            reportDefault ?
                                            ReportDefault::T :
                                            ReportDefault::F);
}

std::string
api::persistConfiguration(const bool reportDefault)
{
    return VolManager::get()->persistConfiguration(reportDefault ?
                                                   ReportDefault::T :
                                                   ReportDefault::F);
}

bool
api::checkConfiguration(const fs::path& path,
                        ConfigurationReport& c_rep)
{
    return VolManager::get()->checkConfiguration(path,
                                                 c_rep);
}

uint64_t
api::getStored(const vd::VolumeId& volName)
{
    return VolManager::get()->findVolume_(volName)->getTotalBackendSize();
}

bool
api::getVolumeDriverReadOnly()
{
    return VolManager::get()->readOnlyMode();
}

void
api::setClusterCacheBehaviour(const vd::VolumeId& volName,
                              const boost::optional<vd::ClusterCacheBehaviour> b)
{
    VolManager::get()->findVolume_(volName)->set_cluster_cache_behaviour(b);
}


boost::optional<vd::ClusterCacheBehaviour>
api::getClusterCacheBehaviour(const vd::VolumeId& volName)
{
    return VolManager::get()->findVolume_(volName)->get_cluster_cache_behaviour();
}

void
api::setClusterCacheMode(const vd::VolumeId& volName,
                         const boost::optional<vd::ClusterCacheMode>& m)
{
    VolManager::get()->findVolume_(volName)->set_cluster_cache_mode(m);
}


boost::optional<vd::ClusterCacheMode>
api::getClusterCacheMode(const vd::VolumeId& volName)
{
    return VolManager::get()->findVolume_(volName)->get_cluster_cache_mode();
}

void
api::setClusterCacheLimit(const vd::VolumeId& volName,
                          const boost::optional<vd::ClusterCount>& l)
{
    VolManager::get()->findVolume_(volName)->set_cluster_cache_limit(l);
}

boost::optional<vd::ClusterCount>
api::getClusterCacheLimit(const vd::VolumeId& volName)
{
    return VolManager::get()->findVolume_(volName)->get_cluster_cache_limit();
}

std::vector<scrubbing::ScrubWork>
api::getScrubbingWork(const vd::VolumeId& volName,
                      const boost::optional<vd::SnapshotName>& start_snap,
                      const boost::optional<vd::SnapshotName>& end_snap)
{
    return VolManager::get()->findVolume_(volName)->getScrubbingWork(start_snap,
                                                                     end_snap);
}

boost::optional<be::Garbage>
api::applyScrubbingWork(const vd::VolumeId& volName,
                        const scrubbing::ScrubReply& scrub_reply,
                        const vd::ScrubbingCleanup cleanup)
{
    SharedVolumePtr v = VolManager::get()->findVolume_(volName);
    return v->applyScrubbingWork(scrub_reply,
                                 cleanup);
}

uint64_t
api::volumePotential(const vd::ClusterSize c,
                     const vd::SCOMultiplier s,
                     const boost::optional<vd::TLogMultiplier>& t)
{
    return VolManager::get()->volumePotential(c,
                                              s,
                                              t);
}

uint64_t
api::volumePotential(const backend::Namespace& nspace)
{
    return VolManager::get()->volumePotential(nspace);
}

backend::BackendConnectionManagerPtr
api::backend_connection_manager()
{
    return VolManager::get()->getBackendConnectionManager();
}

backend::GarbageCollectorPtr
api::backend_garbage_collector()
{
    return VolManager::get()->backend_garbage_collector();
}

void
api::setSyncIgnore(const vd::VolumeId& volName,
                   const uint64_t number_of_syncs_to_ignore,
                   const uint64_t maximum_time_to_ignore_syncs_in_seconds)
{
    return VolManager::get()->findVolume_(volName)->setSyncIgnore(number_of_syncs_to_ignore,
                                                                  maximum_time_to_ignore_syncs_in_seconds);
}

void
api::getSyncIgnore(const vd::VolumeId& volName,
                   uint64_t& number_of_syncs_to_ignore,
                   uint64_t& maximum_time_to_ignore_syncs_in_seconds)
{
    return VolManager::get()->findVolume_(volName)->getSyncIgnore(number_of_syncs_to_ignore,
                                                                  maximum_time_to_ignore_syncs_in_seconds);
}

void
api::setSCOMultiplier(const vd::VolumeId& volName,
                      const vd::SCOMultiplier sco_mult)
{
    VolManager::get()->findVolume_(volName)->setSCOMultiplier(sco_mult);
}

SCOMultiplier
api::getSCOMultiplier(const vd::VolumeId& volName)
{
    return VolManager::get()->findVolume_(volName)->getSCOMultiplier();
}

void
api::setTLogMultiplier(const vd::VolumeId& volName,
                       const boost::optional<vd::TLogMultiplier>& tlog_mult)
{
    VolManager::get()->findVolume_(volName)->setTLogMultiplier(tlog_mult);
}

boost::optional<TLogMultiplier>
api::getTLogMultiplier(const vd::VolumeId& volName)
{
    return VolManager::get()->findVolume_(volName)->getTLogMultiplier();
}

void
api::setSCOCacheMaxNonDisposableFactor(const vd::VolumeId& volName,
                                       const boost::optional<vd::SCOCacheNonDisposableFactor>& max_non_disposable_factor)
{
    VolManager::get()->findVolume_(volName)->setSCOCacheMaxNonDisposableFactor(max_non_disposable_factor);
}

boost::optional<SCOCacheNonDisposableFactor>
api::getSCOCacheMaxNonDisposableFactor(const vd::VolumeId& volName)
{
    return VolManager::get()->findVolume_(volName)->getSCOCacheMaxNonDisposableFactor();
}

void
api::setMetaDataCacheCapacity(const vd::VolumeId& volName,
                              const boost::optional<size_t>& npages)
{
    ASSERT_LOCKABLE_LOCKED(getManagementMutex());
    return VolManager::get()->findVolume_(volName)->set_metadata_cache_capacity(npages);
}

bool
api::fencing_support()
{
    return backend_connection_manager()->config().unique_tag_support();
}

// Local Variables: **
// mode: c++ **
// End: **
