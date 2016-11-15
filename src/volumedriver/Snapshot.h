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

#ifndef _SNAPSHOT_H_
#define _SNAPSHOT_H_

#include "SnapshotName.h"
#include "TLog.h"

#include <string>
#include <vector>

#include <boost/serialization/vector.hpp>
#include <boost/version.hpp>

#include <youtils/IOException.h>

VD_BOOLEAN_ENUM(IncludingStartSnapshot);
VD_BOOLEAN_ENUM(IncludingEndSnapshot);

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
    Snapshot(const SnapshotNum,
             const SnapshotName&,
             const TLogs&,
             const SnapshotMetaData& = SnapshotMetaData(),
             const UUID& = UUID(),
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

    const SnapshotName&
    getName() const
    {
        return static_cast<const SnapshotName&>(name);
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
    getReversedTLogsOnBackendSinceLastCork(const boost::optional<youtils::UUID>&,
                                           OrderedTLogIds&) const;

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
    const SnapshotName name;
    const UUID uuid_;
    const SnapshotMetaData metadata_;
    bool scrubbed;
    const std::string date;
    const bool hasUUIDSpecified_;
    const UUID cork_uuid_;

#if BOOST_VERSION == 105800
public:
#endif
    // only to be used for deserialization
    Snapshot();
#if BOOST_VERSION == 105800
private:
#endif

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

            {
                std::string s;
                ar & make_nvp("name",
                              s);
                const_cast<SnapshotName&>(name) = SnapshotName(s);
            }

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
            ar & make_nvp("name",
                          static_cast<const std::string>(name));
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
    setTLogWrittenToBackend(const TLogId&,
                            bool);

    bool
    isTLogWrittenToBackend(const TLogId&) const;

    bool
    getTLogsInSnapshot(const SnapshotNum,
                       OrderedTLogIds&) const;

    SnapshotNum
    getNextSnapshotNum() const;

    bool
    snapshotExists(const SnapshotName&) const;

    bool
    snapshotExists(const SnapshotNum) const;

    bool
    snapshotExists(const UUID&) const;

    const Snapshot&
    getSnapshot(const SnapshotName&) const;

    const Snapshot&
    getSnapshot(const SnapshotNum) const;

    const Snapshot&
    getSnapshot(const UUID&) const;

    void
    getSnapshotsTill(SnapshotNum,
                     std::vector<SnapshotNum>&,
                     bool including) const;

    void
    getSnapshotsAfter(SnapshotNum,
                      std::vector<SnapshotNum>&) const;

    void
    deleteSnapshot(const SnapshotNum,
                   TLogs& current_tlogs);

    void
    deleteAllButLastSnapshot();

    bool
    snip(const TLogId&,
         const boost::optional<uint64_t>& backend_size);

    uint64_t
    getTotalBackendSize() const;

    uint64_t
    getBackendSize(const SnapshotName& end_snapshot,
                   const boost::optional<SnapshotName>& start_snapshot) const;

    void
    getTLogsTillSnapshot(const SnapshotNum,
                         OrderedTLogIds&) const;

    void
    getTLogsAfterSnapshot(SnapshotNum,
                          OrderedTLogIds&) const;

    void
    getTLogsBetweenSnapshots(const SnapshotNum start,
                             const SnapshotNum end,
                             OrderedTLogIds& out,
                             IncludingEndSnapshot) const;

    void
    deleteTLogsAndSnapshotsAfterSnapshot(SnapshotNum);

    void
    getAllSnapshots(std::vector<SnapshotNum>&) const;

    void
    replace(const OrderedTLogIds& in,
            const std::vector<TLog>& out,
            const SnapshotNum num);

    void
    getSnapshotScrubbingWork(const boost::optional<SnapshotName>& start_snap,
                             const boost::optional<SnapshotName>& end_snap,
                             SnapshotWork& out) const;

    bool
    tlogReferenced(const TLogId&) const;

    boost::optional<TLogId>
    checkAndGetAllTLogsWrittenToBackendAndRemoveLaterOnes(OrderedTLogIds&);

    bool
    hasUUIDSpecified() const
    {
        return size() == 0 or
            begin()->hasUUIDSpecified();
    }

    const UUID&
    getUUID(SnapshotNum) const;

    bool
    hasSnapshotWithUUID(const UUID&) const;

    bool
    checkSnapshotUUID(const SnapshotName&,
                      const UUID&) const;

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
    find_(const SnapshotName&) const;

    iterator
    find_(const SnapshotName&);

    const_iterator
    find_(const SnapshotNum) const;

    iterator
    find_(const SnapshotNum);

    const_iterator
    find_(const UUID&) const;

    iterator
    find_(const UUID&);

    template<typename T>
    const_iterator
    find_or_throw_(const T& t) const
    {
        auto it = find_(t);
        if (it == end())
        {
            std::stringstream ss;
            ss << "Could not find snapshot " << t;
            std::string msg = ss.str();
            LOG_ERROR(msg);
            throw SnapshotNotFoundException(msg.c_str());
        }

        return it;
    }

    template<typename T>
    iterator
    find_or_throw_(const T& t)
    {
        auto it = find_(t);
        if (it == end())
        {
            std::stringstream ss;
            ss << "Could not find snapshot " << t;
            std::string msg = ss.str();
            LOG_ERROR(msg);
            throw SnapshotNotFoundException(msg.c_str());
        }

        return it;
    }

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
