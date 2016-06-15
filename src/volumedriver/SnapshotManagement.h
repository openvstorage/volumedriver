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

#ifndef SNAPSHOTMANAGEMENT_H_
#define SNAPSHOTMANAGEMENT_H_

#include "ClusterLocation.h"
#include "ClusterLocationAndHash.h"
#include "RestartContext.h"
#include "ScrubId.h"
#include "SnapshotName.h"
#include "SnapshotPersistor.h"
#include "VolumeBackPointer.h"
#include "VolumeConfig.h"

#include <time.h>

#include <iosfwd>
#include <limits>
#include <list>
#include <memory>
#include <stdexcept>

#include <boost/thread/mutex.hpp>

#include <youtils/Assert.h>

namespace volumedrivertest
{
class SnapshotManagementTest;
}

namespace volumedriver
{

class TLogWriter;
class TLogBackendWriter;
class WriteOnlyVolume;

/**
 * service for managing the snapshots. It is backed by a /snapshots.xml file.
 */

class SnapshotManagement
    : public VolumeBackPointer
{
public:
    // This is here for a technical reason. SnapshotManagement in the constructor of volume
    // before the backend interface. This means that volume has to schedule the first snapshots
    // to backend, which is a private function
    friend class Volume;
    friend class WriteOnlyVolume;
    friend class ::volumedrivertest::SnapshotManagementTest;
    friend class SnapshotWriter;

    static bool
    exists(const boost::filesystem::path& dir);

    SnapshotManagement(const VolumeConfig&,
                       const RestartContext);

    SnapshotManagement(const SnapshotManagement&) = delete;

    SnapshotManagement& operator=(const SnapshotManagement&) = delete;

    void
    initialize(VolumeInterface* v);

    ~SnapshotManagement();

    // New stuff
    bool
    snapshotsEmpty()
    {
        return sp->snapshotsEmpty();
    }

    void
    listSnapshots(std::list<SnapshotName>& snapshots) const;

    void
    deleteSnapshot(const SnapshotName&);

    SnapshotNum
    getSnapshotNumberByName(const SnapshotName&) const;

    Snapshot
    getSnapshot(const SnapshotName&) const;

    OrderedTLogIds
    getAllTLogs() const;

    bool
    getTLogsInSnapshot(SnapshotNum,
                       OrderedTLogIds&) const;

    bool
    getTLogsInSnapshot(const SnapshotName&,
                       OrderedTLogIds&) const;

    void
    getTLogsNotWrittenToBackend(OrderedTLogIds& out) const;

    OrderedTLogIds
    getTLogsNotWrittenToBackend() const
    {
        OrderedTLogIds tlogs;
        getTLogsNotWrittenToBackend(tlogs);
        return tlogs;
    }

    void
    getTLogsWrittenToBackend(OrderedTLogIds& out) const;

    OrderedTLogIds
    getTLogsWrittenToBackend() const
    {
        OrderedTLogIds tlogs;
        getTLogsWrittenToBackend(tlogs);
        return tlogs;
    }

    bool
    isTLogWrittenToBackend(const TLogId& name) const;

    bool
    snapshotExists(SnapshotNum num) const;

    bool
    snapshotExists(const SnapshotName& name) const;

    void
    eraseSnapshotsAndTLogsAfterSnapshot(SnapshotNum num);

    uint64_t
    getSnapshotBackendSize(const SnapshotName& name) const;

    uint64_t
    getCurrentBackendSize() const;

    uint64_t
    getTotalBackendSize() const;

    bool
    currentTLogHasData();

    template<typename Accumulator>
    Accumulator&
    vold(Accumulator& accumulator,
         BackendInterfacePtr bi,
         const SnapshotName& snap_name = SnapshotName(),
         SCOCloneID start = SCOCloneID(0)) const
    {
        return sp->vold(accumulator,
                        std::move(bi),
                        snap_name,
                        start);
    }

    static std::unique_ptr<SnapshotPersistor>
    createSnapshotPersistor(BackendInterfacePtr bi);

    static void
    writeSnapshotPersistor(const SnapshotPersistor&, BackendInterfacePtr bi);

    void
    destroy(const DeleteLocalData delete_snaps);

    SnapshotName
    getLastSnapshotName() const;

    void
    sync(const MaybeCheckSum& maybe_sco_crc);

    void
    addClusterEntry(const ClusterAddress address,
                    const ClusterLocationAndHash& location_and_hash);

    void
    addSCOCRC(const CheckSum& t);

    ScrubId
    replaceTLogsWithScrubbedOnes(const OrderedTLogIds& /*in*/,
                                 const std::vector<TLog>& /*out*/,
                                 SnapshotNum /*relatedSnapshot*/);

    void
    tlogWrittenToBackendCallback(const TLogId& tlogcounter,
                             const SCO sconame);

    const MaybeParentConfig&
    parent() const
    {
        return sp->parent();
    }

    bool
    isSnapshotInBackend(const SnapshotNum num) const;

    bool
    lastSnapshotOnBackend() const;

    void
    setSnapshotScrubbed(const SnapshotName&);

    const boost::filesystem::path&
    getTLogsPath() const;

    OrderedTLogIds
    getCurrentTLogs() const;

    const TLogId&
    getCurrentTLogId() const;

    boost::filesystem::path
    getCurrentTLogPath() const;

    boost::filesystem::path
    tlogPathPrepender(const std::string&) const;

    boost::filesystem::path
    tlogPathPrepender(const TLogId&) const;

    std::vector<boost::filesystem::path>
    tlogPathPrepender(const OrderedTLogIds&) const;

    OrderedTLogIds
    getTLogsTillSnapshot(SnapshotNum) const;

    OrderedTLogIds
    getTLogsAfterSnapshot(SnapshotNum) const;

    // Don't use this. You probably want to enrich the snapshotmanagement api
    const SnapshotPersistor&
    getSnapshotPersistor() const
    {
        return *sp;
    }

    boost::filesystem::path
    saveSnapshotToTempFile();

    void
    getSnapshotScrubbingWork(const boost::optional<SnapshotName>& start_snap,
                             const boost::optional<SnapshotName>& end_snap,
                             SnapshotWork& out) const;

    void
    scheduleWriteSnapshotToBackend();

    void
    trimToBackend();

    ClusterLocation
    getLastSCOInBackend();

    ClusterLocation
    getLastSCO();

    bool
    tlogReferenced(const TLogId&) const;

    unsigned
    maxTLogEntries() const
    {
        return maxTLogEntries_;
    }

    const youtils::UUID&
    getSnapshotCork(const SnapshotName& snapshot_name) const;

    void
    setAsTemplate(const MaybeCheckSum& maybe_sco_crc);

    boost::optional<youtils::UUID>
    lastCork() const;

    ScrubId
    scrub_id() const;

    ScrubId
    new_scrub_id();

    static boost::filesystem::path
    snapshots_file_path(const boost::filesystem::path& snaps_path);

private:
    DECLARE_LOGGER("SnapshotManagement");

    std::unique_ptr<SnapshotPersistor> sp;
    std::unique_ptr<TLogWriter> currentTLog_;

    //    static const char* snapshotConfigName_;
    TLogId currentTLogId_;

    unsigned maxTLogEntries_;
    unsigned numTLogEntries_;

    time_t previous_tlog_time_;

    // lock order: snap before tlog
    typedef boost::mutex lock_type;
    mutable lock_type snapshot_lock_;
    mutable lock_type tlog_lock_;

    boost::filesystem::path snapshotPath_;
    boost::filesystem::path tlogPath_;
    const backend::Namespace nspace_;

    boost::filesystem::path
    snapshots_file_path_() const;

    // To be called by Volume on local restart
    void
    scheduleTLogsToBeWrittenToBackend();

    void
    syncTLog_(const MaybeCheckSum& maybe_sco_crc);

    CheckSum
    closeTLog_(const boost::filesystem::path* pth = 0);

    void
    maybeCloseTLog_();

    void
    openTLog_();

    void
    scheduleWriteTLogToBackend(const TLogId&,
                               const boost::filesystem::path&,
                               const SCO,
                               const CheckSum&);

    bool
    isSyncedToBackend();

    uint64_t
    getTLogSizes(const OrderedTLogIds&);

    void
    createSnapshot(const SnapshotName& name,
                   const MaybeCheckSum& maybe_sco_crc,
                   const SnapshotMetaData& metadata = SnapshotMetaData(),
                   const UUID& = UUID(),
                   const bool set_scrubbed = false);

    bool
    checkSnapshotUUID(const SnapshotName& snapshotName,
                      const volumedriver::UUID& uuid) const;

    TLogId
    scheduleBackendSync(const MaybeCheckSum& maybe_tlog_crc);

    template<typename T>
    void
    halt_on_error_(T&& op, const char* desc);

    void
    maybe_switch_tlog_();

    void
    set_max_tlog_entries(const boost::optional<TLogMultiplier>& tm,
                         SCOMultiplier sm);
};

}

#endif /* SNAPSHOTMANAGEMENT_H_ */

// Local Variables: **
// mode: c++ **
// End: **
