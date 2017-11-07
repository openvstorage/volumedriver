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

#ifndef _TLOG_H_
#define _TLOG_H_

#include "TLogId.h"
#include "Types.h"

#include <boost/logic/tribool.hpp>
#include <boost/serialization/list.hpp>

#include <youtils/Serialization.h>
#include <youtils/UUID.h>

namespace volumedriver
{

MAKE_EXCEPTION(CorkNotOnBackendException, fungi::IOException);

class TLogs;

class TLog
{
public:
    friend class TLogs;
    friend class SnapshotPersistorTest;

    TLog();

    explicit TLog(const TLogId&);

    ~TLog() = default;

    TLog(const TLog&) = default;

    TLog&
    operator=(const TLog&) = default;

    void
    writtenToBackend(bool);

    bool
    writtenToBackend() const;

    const TLogId&
    id() const
    {
        return uuid;
    }

    TLogName
    getName() const;

    static bool
    isTLogString(const std::string&);

    uint64_t
    backend_size() const
    {
        return size;
    }

    void
    add_to_backend_size(uint64_t s)
    {
        size += s;
    }

    bool
    operator==(const TLog& other) const
    {
        return uuid == other.uuid and
            written_to_backend == other.written_to_backend and
            size == other.size;
    }

    bool
    operator!=(const TLog& other) const
    {
        return not operator==(other);
    }

private:
    DECLARE_LOGGER("TLog");

    friend class boost::serialization::access;
    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar, const unsigned int version)
    {
        if (version >= 1)
        {
            ar & boost::serialization::make_nvp("uuid",
                                                uuid.t);

            if (version == 1)
            {
                ar & boost::serialization::make_nvp("written_to_dss",
                                                    written_to_backend);
            }
            else
            {
                ar & BOOST_SERIALIZATION_NVP(written_to_backend);
            }

            ar & BOOST_SERIALIZATION_NVP(size);
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 2, 2);
        }
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        if(version == 2)
        {
            ar & boost::serialization::make_nvp("uuid",
                                                uuid.t);
            ar & BOOST_SERIALIZATION_NVP(written_to_backend);
            ar & BOOST_SERIALIZATION_NVP(size);
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 2, 2);
        }
    }

    TLogId uuid;
    bool written_to_backend;
    uint64_t size;
};

class TLogs
    : public std::list<TLog>
{
public:
    void
    getOrderedTLogIds(OrderedTLogIds&) const;

    OrderedTLogIds
    getOrderedTLogIds() const
    {
        OrderedTLogIds tlog_ids;
        getOrderedTLogIds(tlog_ids);
        return tlog_ids;
    }

    void
    getReverseOrderedTLogIds(OrderedTLogIds&) const;

    void
    replace(const OrderedTLogIds& in,
            const std::vector<TLog>& out);

    bool
    setTLogWrittenToBackend(const TLogId&,
                            bool = true);

    boost::tribool
    isTLogWrittenToBackend(const TLogId&) const;

    bool
    writtenToBackend() const;

    TLogs
    tlogsOnBackend();

    boost::optional<TLogId>
    checkAndGetAllTLogsWrittenToBackendAndRemoveLaterOnes(OrderedTLogIds&);

    bool
    tlogReferenced(const TLogId&) const;

    bool
    snip(const TLogId&,
         const boost::optional<uint64_t>& backend_size);

    //returns whether cork was seen
    bool
    getReversedTLogsOnBackendSinceLastCork(const boost::optional<youtils::UUID>&,
                                           OrderedTLogIds&) const;

    uint64_t
    backend_size() const;

private:
    DECLARE_LOGGER("TLogs");

    friend class boost::serialization::access;

    BOOST_SERIALIZATION_SPLIT_MEMBER();

    template<class Archive>
    void
    load(Archive& ar,
         const unsigned int /* version */)
    {
        using namespace boost::serialization;

        ar & make_nvp("tlogs",
                      base_object<std::list<TLog> >(*this));
    }

    template<class Archive>
    void
    save(Archive& ar, const unsigned int version) const
    {
        using namespace boost::serialization;

        if(version == 0)
        {
            ar & make_nvp("tlogs",
                          base_object<std::list<TLog> >(*this));
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 0, 0);
        }
    }
};

}

BOOST_CLASS_VERSION(volumedriver::TLog, 2);
BOOST_CLASS_VERSION(volumedriver::TLogs, 0);

#endif // _TLOG_H_

// Local Variables: **
// mode: c++ **
// End: **
