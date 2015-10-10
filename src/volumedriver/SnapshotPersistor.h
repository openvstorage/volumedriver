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

#ifndef SNAPSHOT_PERSITOR_H
#define SNAPSHOT_PERSITOR_H

#include "NSIDMap.h"
#include "ParentConfig.h"
#include "ScrubId.h"
#include "Snapshot.h"
#include "SnapshotName.h"
#include "TLog.h"
#include "Types.h"

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
BOOLEAN_ENUM(WithCurrent)

BOOLEAN_ENUM(SyncAndRename);

BOOLEAN_ENUM(FromOldest);

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

    explicit SnapshotPersistor(const MaybeParentConfig& parent_cfg);

    explicit SnapshotPersistor(const fs::path& path);

    explicit SnapshotPersistor(std::istream& stream);

    explicit SnapshotPersistor(BackendInterfacePtr& bi);

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
    saveToFile(const fs::path& path,
               const SyncAndRename sync_and_rename) const;

    void
    snapshot(const SnapshotName& name,
             const SnapshotMetaData& metadata = SnapshotMetaData(),
             const youtils::UUID& guid = youtils::UUID(),
             const bool create_scrubbed = false);

    bool
    checkSnapshotUUID(const SnapshotName& snapshotName,
                      const volumedriver::UUID& uuid) const;

    /** Return the tlogs in this snapshot in the order they were taken
     */
    bool
    getTLogsInSnapshot(SnapshotNum num,
                       OrderedTLogNames& out) const;

    bool
    isSnapshotInBackend(const SnapshotNum num) const;

    SnapshotNum
    getSnapshotNum(const SnapshotName& name) const;

    SnapshotName
    getSnapshotName(SnapshotNum num) const;

    const Snapshot&
    getSnapshot(const SnapshotName& snapname) const;

    void
    getSnapshotsTill(SnapshotNum num,
                     std::vector<SnapshotNum>&,
                     bool including = true) const;

    void
    getSnapshotsAfter(SnapshotNum num,
                      std::vector<SnapshotNum>&) const;

    bool
    snapshotExists(const SnapshotName& name) const;

    bool
    snapshotExists(SnapshotNum num) const;

    void
    deleteSnapshot(SnapshotNum num);

    void
    getCurrentTLogs(OrderedTLogNames& outTLogs) const;

    OrderedTLogNames
    getCurrentTLogs() const
    {
        OrderedTLogNames tlogs;
        getCurrentTLogs(tlogs);
        return tlogs;
    }

    const MaybeParentConfig&
    parent() const
    {
        return parent_;
    }

    std::string
    getCurrentTLog() const;

    void getTLogsTillSnapshot(const SnapshotName& name,
                              OrderedTLogNames& out) const;

    void
    getTLogsTillSnapshot(SnapshotNum num,
                         OrderedTLogNames& out)  const;

    void
    getTLogsAfterSnapshot(SnapshotNum num,
                          OrderedTLogNames& out) const;

    void
    deleteTLogsAndSnapshotsAfterSnapshot(SnapshotNum num);

    void
    getTLogsBetweenSnapshots(const SnapshotNum start,
                             const SnapshotNum end,
                             OrderedTLogNames& out,
                             IncludingEndSnapshot = IncludingEndSnapshot::T) const;

    void
    getAllSnapshots(std::vector<SnapshotNum>& vec) const;

    void
    getAllTLogs(OrderedTLogNames& vec,
                const WithCurrent with_current) const;

    // DOES NOT SET CURRENT SIZE!!
    void
    trimToBackend();

    void
    getCurrentTLogsWrittenToBackend(OrderedTLogNames&) const;

    OrderedTLogNames
    getCurrentTLogsWrittenToBackend() const
    {
        OrderedTLogNames tlogs;
        getCurrentTLogsWrittenToBackend(tlogs);
        return tlogs;
    }

    void
    setTLogWrittenToBackend(const TLogId& tlogid);

    bool
    isTLogWrittenToBackend(const TLogId& tlogid) const;

    bool
    isTLogWrittenToBackend(const std::string& tlogname) const;

    void
    getTLogsNotWrittenToBackend(OrderedTLogNames& out) const;

    void
    deleteAllButLastSnapshot();

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
    getSnapshotScrubbed(SnapshotNum num,
                        bool nothrow) const;

    void
    setSnapshotScrubbed(SnapshotNum num,
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
    addCurrentBackendSize(uint64_t file_size);

    // Y42 align the backendsize calls.
    uint64_t
    getBackendSize(const SnapshotName& end_snapshot,
                   const boost::optional<SnapshotName>& start_snapshot) const;

    bool
    snip(const std::string& tlog_name,
         const boost::optional<uint64_t>& backend_size);

    bool
    tlogReferenced(const std::string& tlog_name);

    bool
    hasUUIDSpecified() const;

    const youtils::UUID&
    getUUID(const SnapshotNum name) const;

    bool
    hasSnapshotWithUUID(const youtils::UUID& uuid) const;

    const Snapshots&
    getSnapshots() const
    {
        return snapshots;
    }

    ScrubId
    replace(const OrderedTLogNames& in,
            const std::vector<TLog>& out,
            const SnapshotNum num);

    const youtils::UUID&
    getSnapshotCork(const SnapshotName& snapshot_name) const;

    boost::optional<youtils::UUID>
    lastCork() const;

    OrderedTLogNames
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
        if (version < 3)
        {
            THROW_SERIALIZATION_ERROR(version, 3, 3);
        }

        ar & boost::serialization::make_nvp("parent",
                                            parent_);

        ar & BOOST_SERIALIZATION_NVP(current);
        ar & BOOST_SERIALIZATION_NVP(snapshots);

        ar & boost::serialization::make_nvp("scrub_id",
                                            scrub_id_);

        verifySanity_();
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if(version == 3)
        {
            ar & boost::serialization::make_nvp("parent",
                                                parent_);

            ar & BOOST_SERIALIZATION_NVP(current);
            ar & BOOST_SERIALIZATION_NVP(snapshots);

            ar & boost::serialization::make_nvp("scrub_id",
                                                scrub_id_);
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 3, 3);
        }
    }

    static std::string
    getFormatedTime();
};

}

BOOST_CLASS_VERSION(volumedriver::SnapshotPersistor, 3);

#endif // SNAPSHOT_PERSITOR_H

// Local Variables: **
// mode: c++ **
// End: **
