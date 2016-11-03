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

#ifndef WRITEONLYVOLUME_H_
#define WRITEONLYVOLUME_H_

#include "BackendTasks.h"
#include "NSIDMap.h"
#include "PerformanceCounters.h"
#include "TLogReader.h"
#include "SCO.h"
#include "SCOAccessData.h"
#include "Snapshot.h"
#include "SnapshotName.h"
#include "VolumeConfig.h"
#include "VolumeInterface.h"

#include <atomic>
#include <memory>
#include <set>

#include <boost/circular_buffer.hpp>
#include <boost/utility.hpp>

#include <youtils/Logging.h>
#include <youtils/RWLock.h>

namespace scrubbing
{
struct ScrubberResult;
}

namespace volumedriver
{
class SnapshotPersistor;
class ScrubberResult;
class DataStoreNG;
class SnapshotManagement;
class ClusterReadDescriptor;

class WriteOnlyVolume
    : public VolumeInterface
{
    friend class VolumeTestSetup;
    friend class VolManagerTestSetup;
    friend class ErrorHandlingTest;

    friend void backend_task::WriteTLog::run(int);
    // Y42 This is needed cause the TCBTMetadataStore calls back to snapshotManagement.

public:
    DECLARE_LOGGER("WriteOnlyVolume");
    MAKE_EXCEPTION(WriteOnlyVolumeException, fungi::IOException)
    MAKE_EXCEPTION(UnalignedWriteException, WriteOnlyVolumeException);

    WriteOnlyVolume(const VolumeConfig&,
                    const OwnerTag,
                    const boost::shared_ptr<backend::Condition>&,
                    std::unique_ptr<SnapshotManagement>,
                    std::unique_ptr<DataStoreNG>,
                    NSIDMap);

    WriteOnlyVolume(const WriteOnlyVolume&) = delete;
    WriteOnlyVolume& operator=(const WriteOnlyVolume&) = delete;

    virtual ~WriteOnlyVolume();

    const NSIDMap&
    getNSIDMap()
    {
        return nsidmap_;
    }

    uint64_t LBA2Addr(const uint64_t) const;
    ClusterAddress addr2CA(const uint64_t) const;
    uint64_t getSize() const;
    uint64_t getLBASize() const;
    uint64_t getLBACount() const;

    void
    dumpDebugData();

    ClusterMultiplier
    getClusterMultiplier() const
    {
        return cfg_.cluster_mult_;
    }

    SCOMultiplier
    getSCOMultiplier() const override final
    {
        return cfg_.sco_mult_;
    }

    boost::optional<TLogMultiplier>
    getTLogMultiplier() const override final
    {
        return cfg_.tlog_mult_;
    }

    boost::optional<SCOCacheNonDisposableFactor>
    getSCOCacheMaxNonDisposableFactor() const override final
    {
        return cfg_.max_non_disposable_factor_;
    }

    /** @exception IOException */
    void
    validateIOLength(uint64_t lba, uint64_t len) const;

    /** @exception IOException */
    void
    validateIOAlignment(uint64_t lba, uint64_t len) const;

    /** @exception IOException, MetaDataStoreException */
    void
    write(uint64_t lba, const uint8_t *buf, uint64_t len);

    /** @exception IOException */
    void
    sync();

    void
    scheduleBackendSync();

    WriteOnlyVolume*
    newWriteOnlyVolume();

    WriteOnlyVolume*
    backend_restart(SCONumber restartSCO);

    void
    createSnapshot(const SnapshotName&,
                   const SnapshotMetaData& = SnapshotMetaData(),
                   const UUID& = UUID());

    bool
    snapshotExists(const SnapshotName&) const;

    bool
    checkSnapshotUUID(const SnapshotName&,
                      const volumedriver::UUID&) const;

    void
    listSnapshots(std::list<SnapshotName>&) const;

    Snapshot
    getSnapshot(const SnapshotName&) const;

    void
    deleteSnapshot(const SnapshotName&);

    fs::path
    getTempTLogPath() const;

    DataStoreNG*
    getDataStore() override final
    {
        return dataStore_.get();
    }

    // VolumeInterface
    virtual ClusterSize
    getClusterSize() const override final
    {
        return clusterSize_;
    }

    FailOverCacheClientInterface*
    getFailOver() override final
    {
        throw fungi::IOException("Not Implemented");
    }

    virtual const BackendInterfacePtr&
    getBackendInterface(const SCOCloneID id = SCOCloneID(0)) const override final
    {
        return nsidmap_.get(id);
    }

    virtual VolumeInterface*
    getVolumeInterface() override final
    {
        return this;
    }

    virtual void
    halt() override final;

    virtual void
    cork(const youtils::UUID&) override final;

    virtual void
    unCorkAndTrySync(const youtils::UUID&) override final;

    virtual const VolumeId
    getName() const override final;

    // virtual VolumeFailOverState
    // setVolumeFailOverState(VolumeFailOverState /*instate*/) override
    // {
    //     return VolumeFailOverState::OK_STANDALONE;
    // };

    virtual void
    SCOWrittenToBackendCallback(uint64_t file_size,
                                boost::chrono::microseconds write_time) override final;

    virtual void
    tlogWrittenToBackendCallback(const TLogId& tid,
                                 const SCO sconame) override final;

    virtual fs::path
    saveSnapshotToTempFile() override final;

    backend::Namespace
    getNamespace() const override final;

    VolumeConfig
    get_config() const override final;

    // virtual VolumeFailOverState
    // getVolumeFailOverState() const override
    // {
    //     return VolumeFailOverState::OK_STANDALONE;
    // }

    virtual void
    removeUpToFromFailOverCache(const SCO /*sconame*/) override final
    {}

    virtual void
    checkState(const TLogId&) override final
    {};

    virtual void
    metaDataBackendConfigHasChanged(const MetaDataBackendConfig& cfg) override final;

    void
    restoreSnapshot(const SnapshotName&);

    const SnapshotManagement&
    getSnapshotManagement() const;

    uint64_t
    getSnapshotBackendSize(const SnapshotName&);

    uint64_t
    getCurrentBackendSize() const;

    uint64_t
    getTotalBackendSize() const;

    void
    destroy(const RemoveVolumeCompletely,
            uint64_t timeout = 1000);

    bool
    is_halted() const;

    void
    getScrubbingWork(std::vector<std::string>& scrubbing_work_units,
                     const boost::optional<SnapshotName>& start_snap,
                     const boost::optional<SnapshotName>& end_snap) const;

    SnapshotName
    getParentSnapName() const;

    uint64_t
    VolumeDataStoreWriteUsed() const;

    uint64_t
    VolumeDataStoreReadUsed() const;

    uint64_t
    getCacheHits() const;

    uint64_t
    getCacheMisses() const;

    uint64_t
    getNonSequentialReads() const;

    uint64_t
    getTLogUsed() const;

    uint64_t
    getSnapshotSCOCount(const SnapshotName& = SnapshotName());

    // Y42 should be made const
    bool
    isSyncedToBackend() const;

    bool
    isSyncedToBackendUpTo(const SnapshotName&) const;

    // Maximum number of times to try to sync to the FOC
    const static uint32_t max_num_retries = 6000;

private:
    // LOCKING:
    // - rwlock allows concurrent reads / one write
    // - write_lock_ is required to prevent write throttling from delaying reads,
    // -> lock order: write_lock_ before rwlock
    typedef boost::mutex lock_type;
    mutable lock_type write_lock_;

    uint64_t intCeiling(uint64_t x, uint64_t y)
    {
        //rounds x up to nearest multiple of y
        return x % y ?  (x / y + 1) * y : x;
    }

    mutable fungi::RWLock rwlock_;

    bool halted_;

    // eventually we need accessors
    std::unique_ptr<DataStoreNG> dataStore_;

    uint64_t caMask_; // to be used with LBAs!

    const ClusterSize clusterSize_;

    VolumeConfig cfg_;

    std::unique_ptr<SnapshotManagement> snapshotManagement_;

    const unsigned volOffset_;

    const NSIDMap nsidmap_;

    std::vector<ClusterLocation> cluster_locations_;

    fungi::SpinLock volumeStateSpinLock_;

    void
    processReloc_(const TLogName &relocName, bool deletions);

    void
    executeDeletions_(TLogReader &);

    void
    writeClusters_(uint64_t addr, const uint8_t* buf, uint64_t bufsize);

    fs::path
    getCurrentTLogPath() const;

    static bool
    checkSCONamesConsistency_(const std::vector<SCO>& names);

    bool
    checkTLogsConsistency_(CloneTLogs& ctl) const;

    void
    normalizeSAPs_(SCOAccessData::VectorType& sadv);

    void
    checkNotHalted_() const;

    void
    writeClusterMetaData_(ClusterAddress ca,
                          const ClusterLocationAndHash& loc);

    void
    writeConfigToBackend_();

    void
    sync_(AppendCheckSum append_chksum);

    const std::atomic<unsigned>& datastore_throttle_usecs_;
    //    std::atomic<unsigned>& foc_throttle_usecs_;

    void
    throttle_(unsigned throttle_usecs) const;

    bool has_dumped_debug_data;

    fs::path
    ensureDebugDataDirectory();

    void
    maybe_update_owner_tag_(const OwnerTag);
};

} // namespace volumedriver

#endif // !WRITEONLYVOLUME_H_

// Local Variables: **
// mode: c++ **
// End: **
