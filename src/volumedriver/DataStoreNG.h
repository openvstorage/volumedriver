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

#ifndef DATASTORENG_H_
#define DATASTORENG_H_

// DataStore Next Generation (better naming suggestions very welcome)

#include "ClusterLocationAndHash.h"
#include "DataStoreCallBack.h"
#include "OpenSCO.h"
#include "SCO.h"
#include "SCOCache.h"
#include "SCOFetcher.h"
#include "VolumeBackPointer.h"
#include "VolumeConfig.h"
#include "WriteSCOCache.h"

#include <vector>
#include <set>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <youtils/CheckSum.h>
#include <youtils/Continuation.h>
#include <backend/BackendInterface.h>

namespace volumedriver
{

class Volume;
class WriteOnlyVolume;

class VolManagerTestSetup;

// should NOT have a backend interface...
class ClusterReadDescriptor
{
public:
    ClusterReadDescriptor(const ClusterLocationAndHash& loc_and_hash,
                          ClusterAddress ca,
                          uint8_t* buf,
                          BackendInterfacePtr bi)
        : loc_and_hash_(loc_and_hash)
        , ca_(ca)
        , buf_(buf)
        , bi_(std::move(bi))

    { }

    ClusterReadDescriptor(const ClusterReadDescriptor& other) = delete;

    ClusterReadDescriptor(ClusterReadDescriptor&& other)
        : loc_and_hash_(other.loc_and_hash_)
        , ca_(other.ca_)
        , buf_(other.buf_)
        , bi_(std::move(other.bi_))
    {}

    ~ClusterReadDescriptor() = default;

    ClusterReadDescriptor&
    operator=(const ClusterReadDescriptor&) = delete;

    const ClusterLocation&
    getClusterLocation() const
    {
        return loc_and_hash_.clusterLocation;
    }

    ClusterAddress
    getClusterAddress() const
    {
        return ca_;
    }

    uint8_t*
    getBuffer() const
    {
        return buf_;
    }

    // HOPE THIS IS ONLY USED IN 1 THREAD
    BackendInterfacePtr
    getBackendInterface() const
    {
        return bi_ ? bi_->clone() : BackendInterfacePtr();
    }

    const youtils::Weed&
    weed() const
    {
        return loc_and_hash_.weed();
    }

private:
    const ClusterLocationAndHash loc_and_hash_;
    const ClusterAddress ca_;
    uint8_t* buf_;
    std::unique_ptr<BackendInterface> bi_;
};

class DataStoreNG
    : public VolumeBackPointer
    , public DataStoreCallBack
{
    friend class DataStoreNGTest;
    friend class VolManagerTestSetup;
    friend class ErrorHandlingTest;

    typedef boost::archive::text_iarchive iarchive_type;
    typedef boost::archive::text_oarchive oarchive_type;

public:
    DataStoreNG(const VolumeConfig&,
                SCOCache*,
                unsigned num_open_scos);

    ~DataStoreNG();

    DataStoreNG(const DataStoreNG&) = delete;

    DataStoreNG&
    operator=(const DataStoreNG&) = delete;

    void
    initialize(VolumeInterface* vol);

    void
    readClusters(const std::vector<ClusterReadDescriptor>&);

    void
    readClusters(const std::vector<ClusterReadDescriptor>&,
                 youtils::Continuation);

    void
    writeClusterToLocation(const uint8_t* buf,
                           const ClusterLocation& loc,
                           uint32_t& throttle);

    void
    writeClusters(const uint8_t* buf,
                  std::vector<ClusterLocation>& locs,
                  size_t num_locs,
                  uint32_t& throttle_usecs);

    void
    touchCluster(const ClusterLocation&);

    MaybeCheckSum
    sync();

    virtual void
    writtenToBackend(SCO);

    void
    writtenToBackendUpTo(SCO);

    virtual CachedSCOPtr
    getSCO(SCO scoName,
           BackendInterfacePtr,
           const CheckSum* cs = 0);

    virtual void
    reportIOError(SCO sconame,
                  const char* errmsg);

    void
    getAndPushSCOForLocalRestart(SCO sconame,
                                 const CheckSum&);

    MaybeCheckSum
    finalizeCurrentSCO();

    uint64_t
    getClusterSize() const;

    void
    destroy(const DeleteLocalData);

    void
    removeSCO(SCO, bool removeNonDisposable = false);

    void listSCOs(SCONameList &, bool disposable);

    uint64_t
    getWriteUsed();

    uint64_t
    getReadUsed();

    uint64_t
    getCacheHits()
    {
        return cacheHitCounter_;
    }

    uint64_t
    getCacheMisses()
    {
        return cacheMissCounter_;
    }

    const ClusterLocation&
    localRestart(uint64_t nspace_min,
                 uint64_t nspace_max,
                 const SCOAccessData& sad,
                 SCONumber lastSCOInBackend,
                 ClusterLocation lastSCO);

    void
    backend_restart(const SCONumber lastSCOInBackend,
               uint64_t nspace_min,
               uint64_t nspace_max);

    void
    restoreSnapshot(SCONumber num);

    void
    newVolume(uint64_t nspace_min,
              uint64_t nspace_max);

    void
    setSCOMultiplier(const SCOMultiplier);

    SCOMultiplier
    getSCOMultiplier() const;

    // In clusters. We do allow negative values as the ClusterMultiplier can be
    // updated!
    ssize_t
    getRemainingSCOCapacity() const;

    inline SCONumber
    getCurrentSCONumber() const
    {
        return currentClusterLoc_.number();
    }

private:
    //INVARIANT: currentSCO_ == 0 => currentClusterLoc_.getSCOOffset() == 0

    DECLARE_LOGGER("DataStoreNG");

    const ClusterSize cluster_size_;
    SCOMultiplier sco_mult_;

    SCOCache* const scoCache_;
    const Namespace nspace_;
    ClusterLocation currentClusterLoc_;

    WriteSCOCache openSCOs_;
    SCONumber latestSCOnumberInBackend_;

    typedef std::set<SCO> SCONameSet;
    SCONameSet pendingTLogSCOs_;

    // locking:
    // (1) rw_lock protects
    //  * currentClusterLoc_
    //  * openSCOs_
    //  * currentSCO_
    //  * latestSCOnumberInBackend_
    //  * pendingTLogSCOs_
    //  -- to modify any of these, the lock needs to be held exclusively
    //  (2) error_lock_ serializes error reporting
    // Lock order: (1) before (2)

    mutable boost::shared_mutex rw_lock_;
    mutable boost::mutex error_lock_;

    std::atomic<uint64_t> cacheHitCounter_;
    std::atomic<uint64_t> cacheMissCounter_;

    std::unique_ptr<CheckSum> currentCheckSum_;

    OpenSCOPtr
    currentSCO_() const;

    MaybeCheckSum
    sync_();

    void
    validateConfig_();

    void
    pushSCO_(SCO sconame);

    void
    writeCluster_(const uint8_t* buf,
                  ClusterLocation&,
                  bool& sco_full,
                  bool& throttle);

    void
    scanSCOsForBackendRestart_(SCONumber restart);

    CachedSCOPtr
    getSCO_(SCO scoName,
            BackendInterfacePtr,
            bool& cached,
            const CheckSum* = 0);

    // will throw TransientException if cache is full
    void
    updateCurrentSCO_();

    // ignore_transient_errors:
    // If a TransientException is thrown because creation of a new SCO
    // *after successfully finishing!* a previous I/O failed (cache filled up,
    // mountpoint gone) the TransientException should be ignored.
    MaybeCheckSum
    pushAndUpdateCurrentSCO_(bool ignore_transient_errors = true);

    bool
    read_adjacent_clusters_(const ClusterReadDescriptor&,
                            size_t count,
                            bool fetch_if_necessary);

    void
    readFromSCO_(uint8_t* buf,
                 OpenSCOPtr osco,
                 size_t read_size,
                 size_t read_off);

    // never returns, *always* throws a TransientException
    void
    reportIOError_(CachedSCOPtr sco,
                   bool read,
                   const char* errmsg);

    void
    scoWrittenToBackend_(SCO sconame);

    //TODO clean this up
    void
    removeSCO_(SCO sconame,
               bool removeNonDisposable,
               bool updateCurrentLOCandSCO = true);

    void
    writeClusters_(const uint8_t* buf,
                   std::vector<ClusterLocation>& locs,
                   size_t num_locs,
                   uint32_t& throttle_usecs);

};

}

#endif // !DATASTORENG_H_

// Local Variables: **
// mode: c++ **
// End: **
