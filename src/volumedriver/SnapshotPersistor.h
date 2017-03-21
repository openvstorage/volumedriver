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

#ifndef SNAPSHOT_PERSITOR_H
#define SNAPSHOT_PERSITOR_H

#include "NSIDMap.h"
#include "ParentConfig.h"
#include "ScrubId.h"
#include "Snapshot.h"
#include "SnapshotName.h"
#include "TLog.h"
#include "Types.h"
#include "ClusterLocationAndHash.h"

#include <memory>

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/optional.hpp>

#include <youtils/BooleanEnum.h>
#include "youtils/FileUtils.h"
#include <youtils/IOException.h>
#include <youtils/Serialization.h>
#include <youtils/UUID.h>

#include <backend/BackendInterface.h>

namespace toolcut
{
class SnapshotPersistorToolCut;
}
VD_BOOLEAN_ENUM(WithCurrent)

VD_BOOLEAN_ENUM(SyncAndRename);

VD_BOOLEAN_ENUM(FromOldest);

MAKE_EXCEPTION(CorkNotFoundException, fungi::IOException);

namespace volumedriver
{

class SnapshotPersistor
{
public:
    friend class SnapshotManagement;
    friend class SnapshotPersistorTest;
    friend class toolcut::SnapshotPersistorToolCut;

    typedef boost::archive::xml_iarchive iarchive_type;
    typedef boost::archive::xml_oarchive oarchive_type;

    MAKE_EXCEPTION(SnapshotPersistorException, fungi::IOException);
    MAKE_EXCEPTION(SnapshotNameAlreadyExists, SnapshotPersistorException);
    MAKE_EXCEPTION(ExcessiveMetaDataException, SnapshotPersistorException);

    explicit SnapshotPersistor(const MaybeParentConfig&);

    explicit SnapshotPersistor(const fs::path&);

    explicit SnapshotPersistor(std::istream&);

    explicit SnapshotPersistor(BackendInterfacePtr&);

    ~SnapshotPersistor() = default;

    SnapshotPersistor(const SnapshotPersistor&) = default;

    SnapshotPersistor&
    operator=(const SnapshotPersistor&) = delete;

    void
    newTLog();

    std::unique_ptr<SnapshotPersistor>
    parentSnapshotPersistor(BackendInterfacePtr& bi) const;

    template<typename Accumulator>
    Accumulator&
    vold(Accumulator& accumulator,
         BackendInterfacePtr bi,
         const SnapshotName& snapshot_name = SnapshotName(),
         SCOCloneID start = SCOCloneID(0)) const
    {
        if(Accumulator::direction == FromOldest::F)
        {
            accumulator(*this,
                        bi,
                        snapshot_name,
                        start);
        }

        if (parent_)
        {
            parentSnapshotPersistor(bi)->vold(accumulator,
                                              bi->cloneWithNewNamespace(parent_->nspace),
                                              parent_->snapshot,
                                              SCOCloneID(start + 1));
        }

        if(Accumulator::direction == FromOldest::T)
        {
            accumulator(*this,
                        bi,
                        snapshot_name,
                        start);
        }

        return accumulator;
    }

    static const char* nvp_name;

    bool
    snapshotsEmpty()
    {
        return snapshots.empty();
    }

    void
    saveToFile(const fs::path&,
               const SyncAndRename) const;

    void
    snapshot(const SnapshotName&,
             const SnapshotMetaData& = SnapshotMetaData(),
             const youtils::UUID& = youtils::UUID(),
             const bool create_scrubbed = false);

    bool
    checkSnapshotUUID(const SnapshotName&,
                      const volumedriver::UUID&) const;

    /** Return the tlogs in this snapshot in the order they were taken
     */
    bool
    getTLogsInSnapshot(SnapshotNum,
                       OrderedTLogIds&) const;

    bool
    isSnapshotInBackend(const SnapshotNum) const;

    SnapshotNum
    getSnapshotNum(const SnapshotName&) const;

    SnapshotName
    getSnapshotName(SnapshotNum) const;

    const Snapshot&
    getSnapshot(const SnapshotName&) const;

    void
    getSnapshotsTill(SnapshotNum,
                     std::vector<SnapshotNum>&,
                     bool including = true) const;

    void
    getSnapshotsAfter(SnapshotNum,
                      std::vector<SnapshotNum>&) const;

    bool
    snapshotExists(const SnapshotName&) const;

    bool
    snapshotExists(SnapshotNum) const;

    void
    deleteSnapshot(SnapshotNum);

    void
    getCurrentTLogs(OrderedTLogIds&) const;

    OrderedTLogIds
    getCurrentTLogs() const
    {
        OrderedTLogIds tlogs;
        getCurrentTLogs(tlogs);
        return tlogs;
    }

    const MaybeParentConfig&
    parent() const
    {
        return parent_;
    }

    TLogId
    getCurrentTLog() const;

    void getTLogsTillSnapshot(const SnapshotName&,
                              OrderedTLogIds&) const;

    void
    getTLogsTillSnapshot(SnapshotNum,
                         OrderedTLogIds&)  const;

    void
    getTLogsAfterSnapshot(SnapshotNum,
                          OrderedTLogIds&) const;

    void
    deleteTLogsAndSnapshotsAfterSnapshot(SnapshotNum);

    void
    getTLogsBetweenSnapshots(const SnapshotNum start,
                             const SnapshotNum end,
                             OrderedTLogIds& out,
                             IncludingEndSnapshot = IncludingEndSnapshot::T) const;

    void
    getAllSnapshots(std::vector<SnapshotNum>&) const;

    void
    getAllTLogs(OrderedTLogIds&,
                const WithCurrent) const;

    // DOES NOT SET CURRENT SIZE!!
    void
    trimToBackend();

    void
    getCurrentTLogsWrittenToBackend(OrderedTLogIds&) const;

    OrderedTLogIds
    getCurrentTLogsWrittenToBackend() const
    {
        OrderedTLogIds tlogs;
        getCurrentTLogsWrittenToBackend(tlogs);
        return tlogs;
    }

    void
    setTLogWrittenToBackend(const TLogId&,
                            bool on_backend = true);

    bool
    isTLogWrittenToBackend(const TLogId&) const;

    void
    getTLogsNotWrittenToBackend(OrderedTLogIds&) const;

    OrderedTLogIds
    getCurrentTLogsNotWrittenToBackend() const
    {
        OrderedTLogIds tlogs;
        getCurrentTLogsNotWrittenToBackend(tlogs);
        return tlogs;
    }

    void
    getCurrentTLogsNotWrittenToBackend(OrderedTLogIds&) const;

    void
    deleteAllButLastSnapshot();

    OrderedTLogIds
    getTLogsNotWrittenToBackend() const
    {
        OrderedTLogIds tlogs;
        getTLogsNotWrittenToBackend(tlogs);
        return tlogs;
    }

    void
    getTLogsWrittenToBackend(OrderedTLogIds&) const;

    OrderedTLogIds
    getTLogsWrittenToBackend() const
    {
        OrderedTLogIds tlogs;
        getTLogsWrittenToBackend(tlogs);
        return tlogs;
    }

    bool
    getSnapshotScrubbed(SnapshotNum,
                        bool nothrow) const;

    void
    setSnapshotScrubbed(SnapshotNum,
                        bool scrubbed);

    uint64_t
    getCurrentBackendSize() const;

    uint64_t
    getSnapshotBackendSize(const SnapshotName& name) const;

    uint64_t
    getTotalBackendSize() const;

    void
    getSnapshotScrubbingWork(const boost::optional<SnapshotName>& start_snap,
                             const boost::optional<SnapshotName>& end_snap,
                             SnapshotWork& out) const;

    void
    addCurrentBackendSize(uint64_t);

    // Y42 align the backendsize calls.
    uint64_t
    getBackendSize(const SnapshotName& end_snapshot,
                   const boost::optional<SnapshotName>& start_snapshot) const;

    bool
    snip(const TLogId&,
         const boost::optional<uint64_t>& backend_size);

    bool
    tlogReferenced(const TLogId&);

    bool
    hasUUIDSpecified() const;

    const youtils::UUID&
    getUUID(const SnapshotNum) const;

    bool
    hasSnapshotWithUUID(const youtils::UUID&) const;

    const Snapshots&
    getSnapshots() const
    {
        return snapshots;
    }

    ScrubId
    replace(const OrderedTLogIds& in,
            const std::vector<TLog>& out,
            const SnapshotNum num);

    const youtils::UUID&
    getSnapshotCork(const SnapshotName&) const;

    boost::optional<youtils::UUID>
    lastCork() const;

    OrderedTLogIds
    getTLogsOnBackendSinceLastCork(const boost::optional<youtils::UUID>& cork,
                                   const boost::optional<youtils::UUID>& implicit_start_cork) const;

    ScrubId
    scrub_id() const
    {
        return scrub_id_;
    }

    ScrubId
    new_scrub_id()
    {
        scrub_id_ = ScrubId();
        return scrub_id_;
    }

    // push this and the associated test down to Snapshot?
    static constexpr uint32_t max_snapshot_metadata_size = 4096;

private:
    DECLARE_LOGGER("SnapshotPersistor");

    TLogs current;
    Snapshots snapshots;
    MaybeParentConfig parent_;
    ScrubId scrub_id_;

    void
    saveToFileSimple_(const fs::path& path) const;

    friend class boost::serialization::access;
    BOOST_SERIALIZATION_SPLIT_MEMBER();

    void
    verifySanity_() const;

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if (version < 4)
        {
            THROW_SERIALIZATION_ERROR(version, 4, 4);
        }

        ar & boost::serialization::make_nvp("parent",
                                            parent_);

        ar & BOOST_SERIALIZATION_NVP(current);
        ar & BOOST_SERIALIZATION_NVP(snapshots);

        ar & boost::serialization::make_nvp("scrub_id",
                                            scrub_id_);

        bool use_hash = true;
        if (version >= 4)
        {
            ar & boost::serialization::make_nvp("use_hash",
                                                use_hash);
        }
        VERIFY(ClusterLocationAndHash::use_hash() == use_hash);

        verifySanity_();
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if(version == 4)
        {
            ar & boost::serialization::make_nvp("parent",
                                                parent_);

            ar & BOOST_SERIALIZATION_NVP(current);
            ar & BOOST_SERIALIZATION_NVP(snapshots);

            ar & boost::serialization::make_nvp("scrub_id",
                                                scrub_id_);

            bool use_hash = ClusterLocationAndHash::use_hash();
            ar & boost::serialization::make_nvp("use_hash",
                                                use_hash);
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 4, 4);
        }
    }

    static std::string
    getFormatedTime();
};

}

BOOST_CLASS_VERSION(volumedriver::SnapshotPersistor, 4);

#endif // SNAPSHOT_PERSITOR_H

// Local Variables: **
// mode: c++ **
// End: **
