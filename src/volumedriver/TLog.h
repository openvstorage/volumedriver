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

class TLogs;

class TLog
{
public:
    friend class TLogs;
    friend class SnapshotPersistorTest;

    TLog();

    ~TLog() = default;

    TLog(const TLog&) = default;

    TLog&
    operator=(const TLog&) = default;

    void
    writtenToBackend(bool i_wds);

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
    isTLogString(const std::string& in);

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
    getOrderedTLogIds(OrderedTLogIds& out) const;

    void
    getReverseOrderedTLogIds(OrderedTLogIds& out) const;

    void
    replace(const OrderedTLogIds& in,
            const std::vector<TLog>& out);

    bool
    setTLogWrittenToBackend(const TLogId& tid);

    boost::tribool
    isTLogWrittenToBackend(const TLogId& tid) const;

    bool
    writtenToBackend() const;

    TLogs
    tlogsOnDss();

    boost::optional<TLogId>
    checkAndGetAllTLogsWrittenToBackendAndRemoveLaterOnes(OrderedTLogIds&);

    bool
    tlogReferenced(const TLogId&) const;

    bool
    snip(const TLogId&,
         const boost::optional<uint64_t>& backend_size);

    //returns whether cork was seen
    bool
    getReversedTLogsOnBackendSinceLastCork(const boost::optional<youtils::UUID>& cork,
                                           OrderedTLogIds& reverse_vec) const;

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
