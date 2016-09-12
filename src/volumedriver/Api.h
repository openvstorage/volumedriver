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

#ifndef API_H_
#define API_H_

// currently the api leaks too many implementation details - this needs to be
// revised

#include "ClusterCache.h"
#include "ClusterCount.h"
#include "Events.h"
#include "FailOverCacheConfig.h"
#include "MetaDataStoreStats.h"
#include "OwnerTag.h"
#include "PerformanceCounters.h"
#include "SCOCacheInfo.h"
#include "ScrubbingCleanup.h"
#include "Snapshot.h"
#include "SnapshotName.h"
#include "Types.h"
#include "Volume.h"
#include "VolumeConfig.h"
#include "VolumeConfigParameters.h"
#include "VolumeOverview.h"
#include "failovercache/fungilib/Mutex.h" // <-- kill it!

#include <string>
#include <limits>
#include <list>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/variant.hpp>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/Assert.h>
#include <youtils/ConfigurationReport.h>
#include <youtils/UpdateReport.h>
#include <youtils/UUID.h>

#include <backend/GarbageCollectorFwd.h>
#include <backend/Namespace.h>

namespace volumedriver
{

class Volume;
class WriteOnlyVolume;
class MetaDataBackendConfig;

}

class api
{
public:
    static fungi::Mutex&
    getManagementMutex();

    static void
    createNewVolume(const volumedriver::VanillaVolumeConfigParameters& p,
                    const volumedriver::CreateNamespace = volumedriver::CreateNamespace::F);

    static volumedriver::WriteOnlyVolume*
    createNewWriteOnlyVolume(const volumedriver::WriteOnlyVolumeConfigParameters&);

    static void
    createClone(const volumedriver::CloneVolumeConfigParameters&,
                const volumedriver::PrefetchVolumeData,
                const volumedriver::CreateNamespace = volumedriver::CreateNamespace::F);

    static void
    updateMetaDataBackendConfig(volumedriver::WeakVolumePtr,
                                const volumedriver::MetaDataBackendConfig&);

    static void
    updateMetaDataBackendConfig(const volumedriver::VolumeId&,
                                const volumedriver::MetaDataBackendConfig&);

    static volumedriver::WeakVolumePtr
    getVolumePointer(const volumedriver::VolumeId&);

    static boost::optional<volumedriver::WeakVolumePtr>
    get_volume_pointer_no_throw(const volumedriver::VolumeId&);

    static volumedriver::WeakVolumePtr
    getVolumePointer(const volumedriver::Namespace&);

    static const volumedriver::VolumeId
    getVolumeId(volumedriver::WeakVolumePtr);

    static void
    Write(volumedriver::WeakVolumePtr vol,
          uint64_t lba,
          const uint8_t *buf,
          uint64_t buflen);

    static void
    Write(volumedriver::WriteOnlyVolume* vol,
          const uint64_t lba,
          const uint8_t *buf,
          const uint64_t buflen);

    static void
    Read(volumedriver::WeakVolumePtr vol,
         const uint64_t lba,
         uint8_t *buf,
         const uint64_t buflen);

    static void
    Sync(volumedriver::WeakVolumePtr);

    static void
    Resize(volumedriver::WeakVolumePtr,
           uint64_t num_clusters);

    static uint64_t
    GetSize(volumedriver::WeakVolumePtr);

    static uint64_t
    GetLbaSize(volumedriver::WeakVolumePtr);

    static uint64_t
    GetClusterSize(volumedriver::WeakVolumePtr);

    static void
    Init(const boost::filesystem::path& cfg,
         events::PublisherPtr event_publisher = nullptr);

    static void
    Init(const boost::property_tree::ptree& pt,
         events::PublisherPtr event_publisher = nullptr);

    static void
    Exit(void);

    static void
    getVolumeList(std::list<volumedriver::VolumeId>&);

    static void
    getVolumesOverview(std::map<volumedriver::VolumeId,
                       volumedriver::VolumeOverview>& out_map);

    static volumedriver::VolumeConfig
    getVolumeConfig(const volumedriver::VolumeId& volName);

    static void
    local_restart(const volumedriver::Namespace&,
                  const volumedriver::OwnerTag,
                  const volumedriver::FallBackToBackendRestart,
                  const volumedriver::IgnoreFOCIfUnreachable);

    static void
    backend_restart(const volumedriver::Namespace&,
                    const volumedriver::OwnerTag,
                    const volumedriver::PrefetchVolumeData,
                    const volumedriver::IgnoreFOCIfUnreachable);

    static volumedriver::WriteOnlyVolume*
    restartVolumeWriteOnly(const volumedriver::Namespace& ns,
                           const volumedriver::OwnerTag);

    // The destroyVolume flavours MUST NOT be invoked concurrently for the same
    // volume; they can however be invoked concurrently for different volumes.
    static void
    destroyVolume(const volumedriver::VolumeId& volName,
                  const volumedriver::DeleteLocalData delete_local_data,
                  const volumedriver::RemoveVolumeCompletely remove_volume_completely,
                  const volumedriver::DeleteVolumeNamespace delete_volume_namespace,
                  const volumedriver::ForceVolumeDeletion force_volume_deletion);

    static void
    destroyVolume(volumedriver::WeakVolumePtr volName,
                  const volumedriver::DeleteLocalData delete_local_data,
                  const volumedriver::RemoveVolumeCompletely remove_volume_completely,
                  const volumedriver::DeleteVolumeNamespace delete_volume_namespace,
                  const volumedriver::ForceVolumeDeletion force_volume_deletion);

    static void
    destroyVolume(volumedriver::WriteOnlyVolume* vol,
                  const volumedriver::RemoveVolumeCompletely);

    static void
    setAsTemplate(const volumedriver::VolumeId&);

    static void
    removeLocalVolumeData(const backend::Namespace& nspace);

    static volumedriver::SnapshotName
    createSnapshot(volumedriver::WeakVolumePtr,
                   const volumedriver::SnapshotMetaData& = volumedriver::SnapshotMetaData(),
                   const volumedriver::SnapshotName* const snapname = 0,
                   const volumedriver::UUID& uuid = volumedriver::UUID());

    static volumedriver::SnapshotName
    createSnapshot(const volumedriver::VolumeId&,
                   const volumedriver::SnapshotMetaData& = volumedriver::SnapshotMetaData(),
                   const volumedriver::SnapshotName* const snapname = 0);

    static volumedriver::SnapshotName
    createSnapshot(volumedriver::WriteOnlyVolume*,
                   const volumedriver::SnapshotMetaData& = volumedriver::SnapshotMetaData(),
                   const volumedriver::SnapshotName* const snapname = 0,
                   const volumedriver::UUID& uuid = volumedriver::UUID());

    static bool
    checkSnapshotUUID(const volumedriver::WriteOnlyVolume*,
                      const volumedriver::SnapshotName&,
                      const volumedriver::UUID& uuid);

    static bool
    snapshotExists(const volumedriver::WriteOnlyVolume*,
                   const volumedriver::SnapshotName&);

    static void
    showSnapshots(const volumedriver::VolumeId&,
                  std::list<volumedriver::SnapshotName>&);

    static void
    showSnapshots(const volumedriver::WriteOnlyVolume*,
                  std::list<volumedriver::SnapshotName>&);

    static volumedriver::Snapshot
    getSnapshot(const volumedriver::VolumeId&,
                const volumedriver::SnapshotName&);

    static volumedriver::Snapshot
    getSnapshot(const volumedriver::WeakVolumePtr,
                const volumedriver::SnapshotName&);

    static volumedriver::Snapshot
    getSnapshot(const volumedriver::WriteOnlyVolume*,
                const volumedriver::SnapshotName&);

    static void
    destroySnapshot(const volumedriver::VolumeId&,
                    const volumedriver::SnapshotName&);

    static void
    restoreSnapshot(const volumedriver::VolumeId&,
                    const volumedriver::SnapshotName&);

    static void
    restoreSnapshot(volumedriver::WriteOnlyVolume*,
                    const volumedriver::SnapshotName&);

    static uint64_t
    VolumeDataStoreWriteUsed(const volumedriver::VolumeId&);

    static uint64_t
    VolumeDataStoreReadUsed(const volumedriver::VolumeId&);

    static volumedriver::PerformanceCounters&
    performance_counters(const volumedriver::VolumeId& volName);

    static uint64_t
    getCacheHits(const volumedriver::VolumeId&);

    static uint64_t
    getCacheMisses(const volumedriver::VolumeId&);

    static uint64_t
    getNonSequentialReads(const volumedriver::VolumeId& volName);

    static uint64_t
    getClusterCacheHits(const volumedriver::VolumeId&);

    static uint64_t
    getClusterCacheMisses(const volumedriver::VolumeId&);

    static uint64_t
    getQueueCount(const volumedriver::VolumeId&);

    static uint64_t
    getQueueSize(const volumedriver::VolumeId& volName);

    static uint64_t
    getTLogUsed(const volumedriver::VolumeId&);

    static volumedriver::TLogId
    scheduleBackendSync(const volumedriver::VolumeId&);

    static uint32_t
    getMaxTimeBetweenTLogs();

    static uint32_t
    getNumberOfSCOSBetweenCheck();

    static void
    setMaxTimeBetweenTLogs(uint32_t max_time_between_tlogs);

    static bool
    isVolumeSynced(const volumedriver::VolumeId&);

    static bool
    isVolumeSyncedUpTo(const volumedriver::VolumeId&,
                       const volumedriver::SnapshotName&);

    static bool
    isVolumeSyncedUpTo(const volumedriver::WriteOnlyVolume*,
                       const volumedriver::SnapshotName&);

    static bool
    isVolumeSyncedUpTo(const volumedriver::VolumeId&,
                       const volumedriver::TLogId&);

    static void
    removeNamespaceFromSCOCache(const backend::Namespace&);

    static std::string
    getRevision();

    static void
    getSCOCacheMountPointsInfo(volumedriver::SCOCacheMountPointsInfo& info);

    static volumedriver::SCOCacheNamespaceInfo
    getVolumeSCOCacheInfo(const backend::Namespace);

    static volumedriver::ClusterSize
    getDefaultClusterSize();

    static void
    getClusterCacheDeviceInfo(volumedriver::ClusterCache::ManagerType::Info& info);

    static volumedriver::ClusterCacheVolumeInfo
    getClusterCacheVolumeInfo(const volumedriver::VolumeId& volName);

    static volumedriver::VolumeFailOverState
    getFailOverMode(const volumedriver::VolumeId&);

    static void
    setFailOverCacheConfig(const volumedriver::VolumeId&,
                           const boost::optional<volumedriver::FailOverCacheConfig>&);

    static boost::optional<volumedriver::FailOverCacheConfig>
    getFailOverCacheConfig(const volumedriver::VolumeId& volname);

    static uint64_t
    getCurrentSCOCount(const volumedriver::VolumeId&);

    static bool
    getHalted(const volumedriver::VolumeId&);

    static uint64_t
    getSnapshotSCOCount(const volumedriver::VolumeId&,
                        const volumedriver::SnapshotName&);

    static void
    setFOCTimeout(const volumedriver::VolumeId&,
                  const boost::chrono::seconds);

    static bool
    checkVolumeConsistency(const volumedriver::VolumeId&);

    static uint64_t
    getSnapshotBackendSize(const volumedriver::VolumeId&,
                           const volumedriver::SnapshotName&);

    static uint64_t
    getCurrentBackendSize(const volumedriver::VolumeId&);

    static bool
    volumeExists(const volumedriver::VolumeId&);

    static void
    dumpSCOInfo(const std::string& outfile);

    static void
    addSCOCacheMountPoint(const std::string& path,
                          uint64_t size);

    static void
    removeSCOCacheMountPoint(const std::string& path);

    static void
    addClusterCacheDevice(const std::string& path,
                          const uint64_t size);

    static void
    removeClusterCacheDevice(const std::string& path);

    static void
    setClusterCacheDeviceOffline(const std::string& path);

    static void
    setClusterCacheDeviceOnline(const std::string& path);

    static boost::variant<youtils::UpdateReport,
                          youtils::ConfigurationReport>
    updateConfiguration(const boost::property_tree::ptree&);

    static boost::variant<youtils::UpdateReport,
                          youtils::ConfigurationReport>
    updateConfiguration(const boost::filesystem::path&);

    static void
    persistConfiguration(const boost::filesystem::path&,
                         const bool reportDefault);

    static std::string
    persistConfiguration(const bool reportDefault);

    static void
    persistConfiguration(boost::property_tree::ptree&,
                         const bool reportDefault);

    static bool
    checkConfiguration(const boost::filesystem::path&,
                       volumedriver::ConfigurationReport& rep);

    static void
    startRecording();

    static void
    stopRecording();

    static void
    startPrefetching(const volumedriver::VolumeId& volName);

    static void
    getClusterCacheStats(uint64_t& num_hits,
                         uint64_t& num_misses,
                         uint64_t& num_entries);

    static volumedriver::MetaDataStoreStats
    getMetaDataStoreStats(const volumedriver::VolumeId& volname);

    static uint64_t
    getStored(const volumedriver::VolumeId& volName);

    static bool
    getVolumeDriverReadOnly();

    static void
    setClusterCacheBehaviour(const volumedriver::VolumeId&,
                             const boost::optional<volumedriver::ClusterCacheBehaviour>);

    static boost::optional<volumedriver::ClusterCacheBehaviour>
    getClusterCacheBehaviour(const volumedriver::VolumeId&);

    static void
    setClusterCacheMode(const volumedriver::VolumeId&,
                        const boost::optional<volumedriver::ClusterCacheMode>&);

    static boost::optional<volumedriver::ClusterCacheMode>
    getClusterCacheMode(const volumedriver::VolumeId&);

    static void
    setClusterCacheLimit(const volumedriver::VolumeId&,
                         const boost::optional<volumedriver::ClusterCount>&);

    static boost::optional<volumedriver::ClusterCount>
    getClusterCacheLimit(const volumedriver::VolumeId&);

    static std::vector<scrubbing::ScrubWork>
    getScrubbingWork(const volumedriver::VolumeId&,
                     const boost::optional<volumedriver::SnapshotName>& start_snap,
                     const boost::optional<volumedriver::SnapshotName>& end_snap);

    static boost::optional<backend::Garbage>
    applyScrubbingWork(const volumedriver::VolumeId&,
                       const scrubbing::ScrubReply&,
                       const volumedriver::ScrubbingCleanup = volumedriver::ScrubbingCleanup::OnSuccess);

    static uint64_t
    volumePotential(const volumedriver::ClusterSize,
                    const volumedriver::SCOMultiplier,
                    const boost::optional<volumedriver::TLogMultiplier>&);

    static uint64_t
    volumePotential(const backend::Namespace&);

    static backend::BackendConnectionManagerPtr
    backend_connection_manager();

    static backend::GarbageCollectorPtr
    backend_garbage_collector();

    static void
    setSyncIgnore(const volumedriver::VolumeId& volName,
                  const uint64_t number_of_syncs_to_ignore,
                  const uint64_t maximum_time_to_ignore_syncs_in_seconds);

    static void
    getSyncIgnore(const volumedriver::VolumeId& volName,
                       uint64_t& number_of_syncs_to_ignore,
                       uint64_t& maximum_time_to_ignore_syncs_in_seconds);

    static void
    setSCOMultiplier(const volumedriver::VolumeId& volName,
                     const volumedriver::SCOMultiplier sco_mult);

    static volumedriver::SCOMultiplier
    getSCOMultiplier(const volumedriver::VolumeId& volName);

    static void
    setTLogMultiplier(const volumedriver::VolumeId& volName,
                      const boost::optional<volumedriver::TLogMultiplier>& tlog_mult);

    static boost::optional<volumedriver::TLogMultiplier>
    getTLogMultiplier(const volumedriver::VolumeId& volName);

    static void
    setSCOCacheMaxNonDisposableFactor(const volumedriver::VolumeId& volName,
                                      const boost::optional<volumedriver::SCOCacheNonDisposableFactor>& max_non_disposable_factor);

    static boost::optional<volumedriver::SCOCacheNonDisposableFactor>
    getSCOCacheMaxNonDisposableFactor(const volumedriver::VolumeId& volName);

    static void
    setMetaDataCacheCapacity(const volumedriver::VolumeId&,
                             const boost::optional<size_t>& num_pages);

    static bool
    fencing_support();
};

#endif // API_H_

// Local Variables: **
// mode: c++ **
// End: **
