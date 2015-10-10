// Copyright 2015 Open vStorage NV
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
BOOLEAN_ENUM(AbsolutePath)
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
    exists(const fs::path& dir);

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

    void
    getAllTLogs(OrderedTLogNames&,
                const AbsolutePath) const;

    OrderedTLogNames
    getAllTLogs(const AbsolutePath abs) const
    {
        OrderedTLogNames tlogs;
        getAllTLogs(tlogs, abs);
        return tlogs;
    }

    bool
    getTLogsInSnapshot(SnapshotNum num,
                       OrderedTLogNames& out,
                       const AbsolutePath) const;

    bool
    getTLogsInSnapshot(const SnapshotName&,
                       OrderedTLogNames&,
                       const AbsolutePath) const;

    void
    getTLogsNotWrittenToBackend(OrderedTLogNames& out) const;

    OrderedTLogNames
    getTLogsNotWrittenToBackend() const
    {
        OrderedTLogNames tlogs;
        getTLogsNotWrittenToBackend(tlogs);
        return tlogs;
    }

    void
    getTLogsWrittenToBackend(OrderedTLogNames& out) const;

    OrderedTLogNames
    getTLogsWrittenToBackend() const
    {
        OrderedTLogNames tlogs;
        getTLogsWrittenToBackend(tlogs);
        return tlogs;
    }

    bool
    isTLogWrittenToBackend(const TLogName& name) const;

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
    replaceTLogsWithScrubbedOnes(const OrderedTLogNames& /*in*/,
                                 const std::vector<TLog>& /*out*/,
                                 SnapshotNum /*relatedSnapshot*/);

    void
    tlogWrittenToBackendCallback(const TLogID& tlogcounter,
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

    fs::path
    getTLogsPath() const;

    void
    getCurrentTLogs(OrderedTLogNames& tlogs,
                    const AbsolutePath) const;

    const std::string&
    getCurrentTLogName() const;

    fs::path
    getCurrentTLogPath() const;

    fs::path
    makeTLogPath(const std::string& tlogName) const;

    void
    tlogPathPrepender(OrderedTLogNames& vec) const;

    void
    getTLogsTillSnapshot(SnapshotNum num,
                         OrderedTLogNames& out,
                         AbsolutePath absolute) const;

    void
    getTLogsAfterSnapshot(SnapshotNum num,
                          OrderedTLogNames& out,
                          AbsolutePath absolute) const;

    // Don't use this. You probably want to enrich the snapshotmanagement api
    const SnapshotPersistor&
    getSnapshotPersistor() const
    {
        return *sp;
    }

    fs::path
    saveSnapshotToTempFile();

    void
    getSnapshotScrubbingWork(const boost::optional<SnapshotName>& start_snap,
                             const boost::optional<SnapshotName>& end_snap,
                             SnapshotWork& out) const;

    void
    scheduleWriteSnapshotToBackend();

    void trimToDss();

    ClusterLocation
    getLastSCOInBackend();

    ClusterLocation
    getLastSCO();

    bool
    tlogReferenced(const std::string& tlog_name) const;

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

    static fs::path
    snapshots_file_path(const fs::path& snaps_path);

private:
    DECLARE_LOGGER("SnapshotManagement");

    std::unique_ptr<SnapshotPersistor> sp;
    std::unique_ptr<TLogWriter> currentTLog_;

    //    static const char* snapshotConfigName_;
    std::string currentTLogName_;

    unsigned maxTLogEntries_;
    unsigned numTLogEntries_;

    time_t previous_tlog_time_;

    // lock order: snap before tlog
    typedef boost::mutex lock_type;
    mutable lock_type snapshot_lock_;
    mutable lock_type tlog_lock_;

    fs::path snapshotPath_;
    fs::path tlogPath_;
    const backend::Namespace nspace_;

    fs::path
    snapshots_file_path_() const;

    // To be called by Volume on local restart
    void
    scheduleTLogsToBeWrittenToBackend();

    void
    syncTLog_(const MaybeCheckSum& maybe_sco_crc);

    CheckSum
    closeTLog_(const fs::path* pth = 0);

    void
    maybeCloseTLog_();

    void
    openTLog_();

    void
    scheduleWriteTLogToBackend(const std::string& tlogname,
                           const fs::path& tlogpath,
                           const SCO sconame,
                           const CheckSum& tlog_crc);

    bool
    isSyncedToBackend();

    uint64_t
    getTLogSizes(const OrderedTLogNames&);

    void
    createSnapshot(const SnapshotName& name,
                   const MaybeCheckSum& maybe_sco_crc,
                   const SnapshotMetaData& metadata = SnapshotMetaData(),
                   const UUID& = UUID(),
                   const bool set_scrubbed = false);

    bool
    checkSnapshotUUID(const SnapshotName& snapshotName,
                      const volumedriver::UUID& uuid) const;

    TLogID
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
