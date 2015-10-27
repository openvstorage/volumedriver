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

#include "Types.h"

#include <boost/logic/tribool.hpp>
#include <boost/serialization/list.hpp>

#include <youtils/UUID.h>
#include "youtils/Serialization.h"

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

    const TLogID&
    getID() const
    {
        return uuid;
    }

    bool
    hasID(const TLogID& tid) const;

    TLogName
    getName() const
    {
        return getName(uuid);
    }

    static bool
    isTLogString(const std::string& in);

    static TLogName
    getName(const TLogID& tid)
    {
        return "tlog_" + tid.str();
    }

    static TLogID
    getTLogIDFromName(const TLogName& tlogName);

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
            ar & BOOST_SERIALIZATION_NVP(uuid);

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
            ar & BOOST_SERIALIZATION_NVP(uuid);
            ar & BOOST_SERIALIZATION_NVP(written_to_backend);
            ar & BOOST_SERIALIZATION_NVP(size);
        }
        else
        {
            THROW_SERIALIZATION_ERROR(version, 2, 2);
        }
    }

    TLogID uuid;
    bool written_to_backend;
    uint64_t size;
};

class TLogs
    : public std::list<TLog>
{
public:
    void
    getOrderedTLogNames(OrderedTLogNames& out) const;

    void
    getReverseOrderedTLogNames(OrderedTLogNames& out) const;

    void
    replace(const OrderedTLogNames& in,
            const std::vector<TLog>& out);

    bool
    setTLogWrittenToBackend(const TLogID& tid);

    boost::tribool
    isTLogWrittenToBackend(const TLogID& tid) const;

    bool
    writtenToBackend() const;

    TLogs tlogsOnDss();

    void
    getNames(OrderedTLogNames& outTLogs) const;

    TLogName
    checkAndGetAllTLogsWrittenToBackendAndRemoveLaterOnes(OrderedTLogNames&);

    bool
    tlogReferenced(const TLogName& tlog_name) const;

    bool
    snip(const TLogName& tlog,
         const boost::optional<uint64_t>& backend_size);

    //returns whether cork was seen
    bool
    getReversedTLogsOnBackendSinceLastCork(const boost::optional<youtils::UUID>& cork,
                                           OrderedTLogNames& reverse_vec) const;

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
