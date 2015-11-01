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

#ifndef _SNAPSHOT_H_
#define _SNAPSHOT_H_

#include "TLog.h"
#include <youtils/IOException.h>

#include <string>
#include <vector>

#include <boost/serialization/vector.hpp>

BOOLEAN_ENUM(IncludingStartSnapshot);
BOOLEAN_ENUM(IncludingEndSnapshot);

namespace volumedriver
{

MAKE_EXCEPTION(SnapshotNotFoundException, fungi::IOException);

class Snapshots;
class SnapshotPersistorTest;
class SnapshotsTLogsTest;

typedef std::vector<char> SnapshotMetaData;

class Snapshot
    : public TLogs
{
public:
    Snapshot(const SnapshotNum num,
             const std::string& name,
             const TLogs& i_tlogs,
             const SnapshotMetaData& metadata = SnapshotMetaData(),
             const UUID& uuid = UUID(),
             bool set_scrubbed = false);

    const SnapshotNum&
    snapshotNumber() const
    {
        return num;
    }

    uint64_t
    backend_size() const;

    bool
    inBackend() const;

    const std::string&
    getName() const
    {
        return name;
    }

    const UUID&
    getUUID() const
    {
        return uuid_;
    }

    bool
    hasUUIDSpecified() const
    {
        return hasUUIDSpecified_;
    }

    const std::string&
    getDate() const
    {
        return date;
    }

    const UUID&
    getCork() const
    {
        return cork_uuid_;
    }

    //returns whether cork was seen
    bool
    getReversedTLogsOnBackendSinceLastCork(const boost::optional<youtils::UUID>& cork,
                                           OrderedTLogNames& reverse_vec) const;

    const SnapshotMetaData&
    metadata() const
    {
        return metadata_;
    }

    template<typename T>
    void
    for_each_tlog(T&& t) const
    {
        for (const TLog& tlog : *this)
        {
            t(tlog);
        }
    }

private:
    DECLARE_LOGGER("Snapshot");

    friend class boost::serialization::access;
    friend class Snapshots;
    friend class SnapshotPersistor;

    friend class SnapshotPersistorTest;
    friend class SnapshotsTLogsTest;

    const SnapshotNum num;
    const std::string name;
    const UUID uuid_;
    const SnapshotMetaData metadata_;
    bool scrubbed;
    const std::string date;
    const bool hasUUIDSpecified_;
    const UUID cork_uuid_;

    Snapshot()
        : num(0)
        , scrubbed(false)
        , hasUUIDSpecified_(true)
    {};

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        using namespace boost::serialization;

        if (version < 5)
        {
            THROW_SERIALIZATION_ERROR(version, 5, 5);
        }
        else
        {
            ar & make_nvp("tlogs",
                          base_object<TLogs>(*this));
            ar & make_nvp("num",
                          const_cast<SnapshotNum&>(num));
            ar & make_nvp("name",
                          const_cast<std::string&>(name));

            ar & make_nvp("scrubbed",
                          scrubbed);

            ar & make_nvp("date",
                          const_cast<std::string&>(date));
            ar & make_nvp("snapshot-uuid",
                          const_cast<UUID&>(uuid_));

            ar & make_nvp("cork_uuid",
                          const_cast<UUID&>(cork_uuid_));

            ar & make_nvp("metadata",
                          const_cast<SnapshotMetaData&>(metadata_));
        }
    }

    template<class Archive>
    void
    save(Archive& ar,
         const unsigned int version) const
    {
        using namespace boost::serialization;

        if(version != 5)
        {
            THROW_SERIALIZATION_ERROR(version, 5, 5);
        }
        else
        {
            ar & make_nvp("tlogs",
                          base_object<TLogs>(*this));
            ar & BOOST_SERIALIZATION_NVP(num);
            ar & BOOST_SERIALIZATION_NVP(name);
            ar & BOOST_SERIALIZATION_NVP(scrubbed);
            ar & BOOST_SERIALIZATION_NVP(date);
            ar & make_nvp("snapshot-uuid", uuid_);
            ar & make_nvp("cork_uuid", const_cast<UUID&>(cork_uuid_));
            ar & make_nvp("metadata", metadata_);
        }
    }
};

class Snapshots
    : public std::list<Snapshot>
{
public:

    bool
    setTLogWrittenToBackend(const TLogID& tid);

    bool
    isTLogWrittenToBackend(const TLogID& tid) const;

    bool
    getTLogsInSnapshot(const SnapshotNum,
                       OrderedTLogNames&) const;

    const UUID&
    getSnapshotCork(const std::string& snapshot_name) const;

    SnapshotNum
    getNextSnapshotNum() const;

    bool
    snapshotExists(const std::string& name) const;

    bool
    snapshotExists(const SnapshotNum num) const;

    SnapshotNum
    getSnapshotNum(const std::string& name) const;

    SnapshotNum
    getSnapshotNumFromUUID(const UUID&) const;

    const std::string&
    getSnapshotName(const SnapshotNum num) const;

    const std::string&
    getSnapshotName(const UUID& uuid) const;

    Snapshot
    getSnapshot(const std::string& snapname) const;

    void
    getSnapshotsTill(SnapshotNum num,
                     std::vector<SnapshotNum>& out,
                     bool including) const;

    void
    getSnapshotsAfter(SnapshotNum num,
                      std::vector<SnapshotNum>& out) const;

    void
    deleteSnapshot(const SnapshotNum,
                   TLogs& current_tlogs);

    void
    deleteAllButLastSnapshot();

    bool
    snip(const std::string tlog_name,
         const boost::optional<uint64_t>& backend_size);

    uint64_t
    getBackendSize(const std::string& name) const;

    uint64_t
    getTotalBackendSize() const;

    uint64_t
    getBackendSize(const std::string& end_snapshot,
                   boost::optional<std::string> start_snapshot) const;

    void
    getTLogsTillSnapshot(const SnapshotNum num,
                         OrderedTLogNames& out) const;

    void
    getTLogsAfterSnapshot(SnapshotNum num,
                          OrderedTLogNames& out) const;

    void
    getTLogsBetweenSnapshots(const SnapshotNum start,
                             const SnapshotNum end,
                             OrderedTLogNames& out,
                             IncludingEndSnapshot) const;

    void
    deleteTLogsAndSnapshotsAfterSnapshot(SnapshotNum num);

    void
    getAllSnapshots(std::vector<SnapshotNum>& vec) const;

    void
    replace(const OrderedTLogNames& in,
            const std::vector<TLog>& out,
            const SnapshotNum num);

    void
    getSnapshotScrubbingWork(const boost::optional<std::string>& start_snap,
                             const boost::optional<std::string>& end_snap,
                             SnapshotWork& out) const;

    bool
    tlogReferenced(const std::string& tlog_name) const;

    TLogName
    checkAndGetAllTLogsWrittenToBackendAndRemoveLaterOnes(OrderedTLogNames&);

    bool
    hasUUIDSpecified() const
    {
        return size() == 0 or
            begin()->hasUUIDSpecified();
    }

    const UUID&
    getUUID(SnapshotNum num) const;

    bool
    hasSnapshotWithUUID(const UUID& uuid) const;

    bool
    checkSnapshotUUID(const std::string& name,
                      const UUID& uuid) const;

    template<typename T>
    void
    for_each_snapshot(T&& t) const
    {
        for (const Snapshot& snap : *this)
        {
            t(snap);
        }
    }

private:
    DECLARE_LOGGER("Snapshots");

    friend class boost::serialization::access;
    friend class SnapshotPersistor;
    friend class SnapshotPersistorTest;
    friend class SnapshotsTLogsTest;

    const_iterator
    find_(const std::string& name) const;

    iterator
    find_(const std::string& name);

    const_iterator
    find_(const SnapshotNum num) const;

    iterator
    find_(const SnapshotNum num);

    const_iterator
    find_(const UUID& uuid) const;

    iterator
    find_(const UUID& uuid);

    template<typename T>
    const_iterator
    find_or_throw_(const T&) const;

    template<typename T>
    iterator
    find_or_throw_(const T&);

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if(version == 0)
        {
            ar & boost::serialization::make_nvp("snapshots",
                                                boost::serialization::base_object<std::list<Snapshot> >(*this));
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if(version == 0)
        {
            ar & boost::serialization::make_nvp("snapshots",
                                                boost::serialization::base_object<std::list<Snapshot> >(*this));
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }
    }
};

}

BOOST_CLASS_VERSION(volumedriver::Snapshot, 5);
BOOST_CLASS_VERSION(volumedriver::Snapshots, 0);

#endif // _SNAPSHOT_H_

// Local Variables: **
// mode: c++ **
// End: **
