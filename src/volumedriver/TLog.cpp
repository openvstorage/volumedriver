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

#include "TLog.h"

#include <boost/bind.hpp>

#include <youtils/Assert.h>
#include <youtils/IOException.h>


namespace volumedriver
{

namespace yt = youtils;

TLog::TLog()
    : written_to_backend(false)
    , size(0)
{}

void
TLog::writtenToBackend(bool in_backend)
{
    written_to_backend = in_backend;
}

bool
TLog::isTLogString(const std::string& in)
{
    return  (in.substr(0,5) == "tlog_")
        and UUID::isUUIDString(in.substr(5));
}

bool
TLog::hasID(const TLogID& tid) const
{
    return tid == uuid;
}

bool
TLog::writtenToBackend() const
{
    return written_to_backend;
}

TLogID
TLog::getTLogIDFromName(const std::string& tlogName)
{
    static const size_t len = sizeof("tlog_") -1;
    return TLogID(tlogName.substr(len));
}

TLogs
TLogs::tlogsOnDss()
{
    TLogs ret;

    for (auto& tl : *this)
    {
        if (tl.writtenToBackend())
        {
            ret.push_back(tl);
        }
        else
        {
            break;
        }
    }
    return ret;
}

void
TLogs::getOrderedTLogNames(OrderedTLogNames& out) const
{
    for(const_iterator it = begin(); it != end(); ++it)
    {
        out.push_back(it->getName());
    }
}

void
TLogs::getReverseOrderedTLogNames(OrderedTLogNames& out) const
{
    for(const_reverse_iterator it = rbegin(); it !=rend(); ++it)
    {
        out.push_back(it->getName());
    }
}

bool
TLogs::tlogReferenced(const std::string& tlog_name) const
{
    for(auto it = begin(); it != end(); ++it)
    {
        if (it->getID() == TLog::getTLogIDFromName(tlog_name))
        {
            return true;
        }
    }
    return false;
}

void
TLogs::replace(const OrderedTLogNames& in,
               const std::vector<TLog>& out)
{
    if(in.size() != size())
    {
        LOG_WARN("not replacing TLogs because snapshot has changed!");
        throw fungi::IOException("In tlogs not equal to out ones");

    }

    size_t i = 0;

    for (const auto& t : *this)
    {
        if(t.getName() != in[i++])
        {
            LOG_WARN("not replacing TLogs because snapshot has changed!");
            throw fungi::IOException("In tlogs not equal to out ones");
        }
    }

    if (out.size() == 0)
    {
        LOG_ERROR("Result of a scrub cannot be empty, refusing replace");
        throw fungi::IOException("Result of a scrub cannot be empty, refusing replace");
    }

    clear();

    for (const auto& t : out)
    {
        push_back(t);
    }
}

bool
TLogs::snip(const std::string& tlogname,
            const boost::optional<uint64_t>& backend_size)
{
    TLogID tid = TLog::getTLogIDFromName(tlogname);

    iterator it = find_if(begin(), end(), boost::bind(&TLog::hasID, _1, tid));

    if(it == end())
    {
        return false;
    }
    else
    {
        if (backend_size)
        {
            it->size = *backend_size;
        }

        erase(++it, end());
        return true;
    }
}

bool
TLogs::setTLogWrittenToBackend(const TLogID& tid)
{
    for (auto& tlog : *this)
    {
        if (tlog.hasID(tid))
        {
            tlog.written_to_backend = true;
            return true;
        }
        else if (tlog.written_to_backend == false)
        {
            // VERIFY() might seem more appropriate here, but that will exit the
            // tester for this very functionality in debug builds.
            const std::string msg("TLog " +
                                  tlog.getName() +
                                  ": unexpected state 'written_to_backend == false'.");
            LOG_ERROR(msg);
            throw fungi::IOException(msg.c_str());
        }
    }

    return false;
}

boost::tribool
TLogs::isTLogWrittenToBackend(const TLogID& tid) const
{
    const_iterator it = find_if(begin(), end(), boost::bind(&TLog::hasID, _1, tid));
    if(it == end())
    {
        return boost::indeterminate;
    }
    else
    {
        return static_cast<bool>(it->writtenToBackend());
    }
}

bool
TLogs::writtenToBackend() const
{
    for(const_iterator i = begin(); i !=end(); ++i)
    {
        if(not i->written_to_backend)
        {
            return false;
        }
    }
    return true;
}

void
TLogs::getNames(OrderedTLogNames& out) const
{
    for(const_iterator it = begin(); it != end(); ++it)
    {
        out.push_back(it->getName());
    }
    return;
}


TLogName
TLogs::checkAndGetAllTLogsWrittenToBackendAndRemoveLaterOnes(OrderedTLogNames& vec)
{
    TLogName retval;
    for(iterator i = begin(); i != end(); ++i)
    {
        if(i->written_to_backend)
        {
            vec.push_back(i->getName());
        }
        else
        {
            retval = i->getName();
            erase(i,end());
            return retval;
        }
    }
    VERIFY(retval.empty());
    return retval;
}

bool
TLogs::getReversedTLogsOnBackendSinceLastCork(const boost::optional<yt::UUID>& cork,
                                              OrderedTLogNames& reverse_vec) const
{
    bool seen_written_to_backend = false;

    for(const_reverse_iterator it = rbegin(); it != rend(); ++it)
    {
        if(cork != boost::none and it->getID() == *cork)
        {
            VERIFY(it->writtenToBackend());
            return true;
        }
        else if(it->writtenToBackend())
        {
            seen_written_to_backend = true;
            reverse_vec.push_back(it->getName());
        }
        else
        {
            VERIFY(not seen_written_to_backend);
        }
    }

    return false;
}

uint64_t
TLogs::backend_size() const
{
    uint64_t s = 0;

    for (const auto& t : *this)
    {
        s += t.backend_size();
    }

    return s;
}

}

// Local Variables: **
// mode: c++ **
// End: **
