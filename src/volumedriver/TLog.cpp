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

TLogName
TLog::getName() const
{
    return boost::lexical_cast<TLogName>(uuid);
}

bool
TLog::isTLogString(const std::string& in)
{
    std::stringstream ss;
    ss << in;
    TLogId id;
    ss >> id;
    return not ss.fail();
}

bool
TLog::writtenToBackend() const
{
    return written_to_backend;
}

TLogs
TLogs::tlogsOnBackend()
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
TLogs::getOrderedTLogIds(OrderedTLogIds& out) const
{
    for(const_iterator it = begin(); it != end(); ++it)
    {
        out.push_back(it->id());
    }
}

void
TLogs::getReverseOrderedTLogIds(OrderedTLogIds& out) const
{
    for(const_reverse_iterator it = rbegin(); it !=rend(); ++it)
    {
        out.push_back(it->id());
    }
}

bool
TLogs::tlogReferenced(const TLogId& tlog_id) const
{
    for(auto it = begin(); it != end(); ++it)
    {
        if (it->id() == tlog_id)
        {
            return true;
        }
    }
    return false;
}

void
TLogs::replace(const OrderedTLogIds& in,
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
        if(t.id() != in[i++])
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
TLogs::snip(const TLogId& tlog_id,
            const boost::optional<uint64_t>& backend_size)
{
    iterator it = find_if(begin(),
                          end(),
                          [&](const TLog& tlog) -> bool
                          {
                              return tlog_id == tlog.id();
                          });

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
TLogs::setTLogWrittenToBackend(const TLogId& tid,
                               bool on_backend)
{
    for (auto& tlog : *this)
    {
        if (tlog.id() == tid)
        {
            tlog.written_to_backend = on_backend;
            return true;
        }
        else if (on_backend and tlog.written_to_backend == false)
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
TLogs::isTLogWrittenToBackend(const TLogId& tid) const
{
    const_iterator it = find_if(begin(),
                                end(),
                                [&](const TLog& tlog) -> bool
                                {
                                    return tid == tlog.id();
                                });
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

boost::optional<TLogId>
TLogs::checkAndGetAllTLogsWrittenToBackendAndRemoveLaterOnes(OrderedTLogIds& vec)
{
    boost::optional<TLogId> tlog_id;

    for(iterator i = begin(); i != end(); ++i)
    {
        if(i->written_to_backend)
        {
            vec.push_back(i->id());
        }
        else
        {
            tlog_id = i->id();
            erase(i, end());
            return tlog_id;
        }
    }

    VERIFY(tlog_id == boost::none);
    return tlog_id;
}

bool
TLogs::getReversedTLogsOnBackendSinceLastCork(const boost::optional<yt::UUID>& cork,
                                              OrderedTLogIds& reverse_vec) const
{
    bool seen_written_to_backend = false;

    for(const_reverse_iterator it = rbegin(); it != rend(); ++it)
    {
        if(cork != boost::none and it->id() == TLogId(*cork))
        {
            if (not it->writtenToBackend())
            {
                throw CorkNotOnBackendException("TLog not on backend",
                                                boost::lexical_cast<std::string>(it->id()).c_str());
            }
            else
            {
                return true;
            }
        }
        else if(it->writtenToBackend())
        {
            seen_written_to_backend = true;
            reverse_vec.push_back(it->id());
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
