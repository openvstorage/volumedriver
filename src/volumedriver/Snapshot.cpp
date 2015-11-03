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

#include "Snapshot.h"

#include <boost/bind.hpp>

#include <youtils/IOException.h>
#include <youtils/Assert.h>

namespace volumedriver
{

namespace yt = youtils;

namespace
{

// XXX:
// Converted from a static member to an anon callable (with a plain function the
// logging would become a headache). Ultimately this has to go - there must be a
// boost or a C++-11 helper for this type of stuff, e.g.:
//
//     auto now(std::chrono::system_clock::now());
//     std::time_t t(std::chrono::system_clock::to_time_t(now));
//     return std::ctime(t);
// ?
struct CurrentTimeAsString
{
    DECLARE_LOGGER("CurrentTimeAsString");

    std::string
    operator()()
    {
        const time_t tim = time(0);
        if(tim == (time_t)(-1))
        {
            LOG_ERROR("Could not get the time");
            return "unknown";
        }

        char buf[128];
        char* res = ctime_r(&tim, buf);
        if(res == 0)
        {
            LOG_ERROR("Could not get the time");
            return "unknown";
        }
        else
        {
            VERIFY(res == buf);
            return std::string(buf);
        }
    }
};

}

Snapshot::Snapshot(const SnapshotNum i_num,
                   const SnapshotName& i_name,
                   const TLogs& tlogs,
                   const SnapshotMetaData& metadata,
                   const UUID& uuid,
                   const bool set_scrubbed)
    : TLogs(tlogs)
    , num(i_num)
    , name(i_name)
    , uuid_(uuid)
    , metadata_(metadata)
    , scrubbed(set_scrubbed)
    , date(CurrentTimeAsString()())
    , hasUUIDSpecified_(true)
    , cork_uuid_(tlogs.empty() ? UUID() : tlogs.back().id())
{}

bool
Snapshot::inBackend() const
{
    for (const auto& tl : *this)
    {
        if (not tl.writtenToBackend())
        {
            return false;
        }
    }
    return true;
}

bool
Snapshot::getReversedTLogsOnBackendSinceLastCork(const boost::optional<yt::UUID>& cork,
                                                 OrderedTLogIds& reverse_vec) const
{
    if (cork != boost::none and
        cork_uuid_ == *cork and
        writtenToBackend())
    {
        return true;
    }

    return TLogs::getReversedTLogsOnBackendSinceLastCork(cork,
                                                         reverse_vec);
}

uint64_t
Snapshot::backend_size() const
{
    uint64_t s = 0;

    for (const auto& t : *this)
    {
        s += t.backend_size();
    }

    return s;
}

bool
Snapshots::tlogReferenced(const TLogId& tlog_id) const
{
    for(auto i = begin(); i != end(); ++i)
    {
        if(i->tlogReferenced(tlog_id))
        {
            return true;
        }
    }
    return false;
}

bool
Snapshots::setTLogWrittenToBackend(const TLogId& tid)
{
    for(iterator i = begin(); i != end(); ++i)
    {
        if (i->setTLogWrittenToBackend(tid))
        {
            return true;
        }
    }
    return false;
}

bool
Snapshots::isTLogWrittenToBackend(const TLogId& tid) const
{
    for(const_iterator i = begin(); i != end(); ++i)
    {
        boost::tribool b = i->isTLogWrittenToBackend(tid);
        if(b)
        {
            return true;
        }
        if(!b)
        {
            return false;
        }
    }
    return false;
}

Snapshots::const_iterator
Snapshots::find_(const SnapshotName& name) const
{
    return find_if(begin(),
                   end(),
                   [&](const Snapshot& snap)
                   {
                       return snap.name == name;
                   });
}

Snapshots::iterator
Snapshots::find_(const SnapshotName& name)
{
    return find_if(begin(),
                   end(),
                   [&](const Snapshot& snap)
                   {
                       return snap.name == name;
                   });
}

Snapshots::const_iterator
Snapshots::find_(const UUID& uuid) const
{
    return find_if(begin(),
                   end(),
                   [&](const Snapshot& snap)
                   {
                       return snap.uuid_ == uuid;
                   });
}

Snapshots::iterator
Snapshots::find_(const UUID& uuid)
{
    return find_if(begin(),
                   end(),
                   [&](const Snapshot& snap)
                   {
                       return snap.uuid_ == uuid;
                   });
}

Snapshots::const_iterator
Snapshots::find_(const SnapshotNum num) const
{
    return find_if(begin(),
                   end(),
                   [&](const Snapshot& snap)
                   {
                       return snap.num == num;
                   });
}

Snapshots::iterator
Snapshots::find_(const SnapshotNum num)
{
    return find_if(begin(),
                   end(),
                   [&](const Snapshot& snap)
                   {
                       return snap.num == num;
                   });
}

bool
Snapshots::getTLogsInSnapshot(const SnapshotNum num,
                              OrderedTLogIds& out) const
{
    auto it = find_(num);
    if(it == end())
    {
        return false;
    }
    else
    {
        it->getOrderedTLogIds(out);
        return true;
    }
}

bool
Snapshots::checkSnapshotUUID(const SnapshotName& name,
                             const UUID& uuid) const
{
    auto it = find_(name);
    if(it == end())
    {
        return false;
    }
    else
    {
        return it->uuid_ == uuid;
    }
}

SnapshotNum
Snapshots::getNextSnapshotNum() const
{
    if(empty())
    {
        return 0;
    }
    else
    {
        return back().num + 1;
    }
}

bool
Snapshots::snapshotExists(const SnapshotName& name) const
{
    return find_(name) != end();
}

bool
Snapshots::snapshotExists(const SnapshotNum num) const
{
    return find_(num) != end();
}

bool
Snapshots::hasSnapshotWithUUID(const UUID& uuid) const
{
    return find_(uuid) != end();
}

const Snapshot&
Snapshots::getSnapshot(const SnapshotName& snapname) const
{
    return *find_or_throw_(snapname);
}

const Snapshot&
Snapshots::getSnapshot(const SnapshotNum num) const
{
    return *find_or_throw_(num);
}

const Snapshot&
Snapshots::getSnapshot(const UUID& uuid) const
{
    return *find_or_throw_(uuid);
}

uint64_t
Snapshots::getTotalBackendSize() const
{
    uint64_t result = 0;

    for(const_iterator it = begin();
        it != end();
        ++it)
    {
        result += it->backend_size();
    }
    return result;
}

uint64_t
Snapshots::getBackendSize(const SnapshotName& end_snapshot,
                          const boost::optional<SnapshotName>& start_snapshot) const
{
    VERIFY(size() > 0);

    uint64_t result = 0;
    const_iterator start = start_snapshot ?
        find_(*start_snapshot) :
        begin();

    VERIFY(start != end());
    if(start_snapshot)
    {
        ++start;
    }

    const_iterator stop = find_(end_snapshot);
    VERIFY(stop != end());
    ++stop;

    for(const_iterator it = start;
        it != stop;
        ++it)
    {
        if(it == end())
        {
            LOG_WARN("rolling of the end of the snapshots list, arguments are probably wrong");
            throw fungi::IOException("Wrong args for getBackendSize()!!!");
        }

        result += it->backend_size();
    }
    return result;
}

void
Snapshots::getSnapshotsTill(SnapshotNum num,
                            std::vector<SnapshotNum>& out,
                            bool including) const
{
    for(const_iterator it = begin(); it != end(); ++it)
    {
        if(it->num == num)
        {
            if(including)
            {
                out.push_back(it->num);
                return;
            }
            else
            {
                return;
            }
        }
        else
        {
            out.push_back(it->num);
        }
    }
    LOG_ERROR("Snapshot with id " << num << " does not exist");
    throw fungi::IOException("Snapshot does not exist");
}

void
Snapshots::getSnapshotsAfter(SnapshotNum num,
                             std::vector<SnapshotNum>& out) const
{
    for(const_reverse_iterator it = rbegin(); it != rend(); ++it)
    {
        if(it->num == num)
        {
            std::reverse(out.begin(),out.end());
            return;
        }
        else
        {
            out.push_back(it->num);
        }
    }

    LOG_ERROR("Snapshot with id " << num << " does not exist");
    throw fungi::IOException("Snapshot does not exist");
}

void
Snapshots::deleteSnapshot(const SnapshotNum num,
                          TLogs& io)
{
    iterator it = find_or_throw_(num);
    iterator it2 = it;

    if(++it2 == end())
    {
        io.splice(io.begin(), *it);
    }
    else
    {
        it2->splice(it2->begin(), *it);
        it2->scrubbed = false;
    }

    erase(it);
}

void
Snapshots::deleteAllButLastSnapshot()
{
    reverse_iterator the_last = rbegin();
    VERIFY(the_last != rend());

    for (reverse_iterator it = ++rbegin(); it != rend(); ++it)
    {
        the_last->splice(the_last->begin(), *it);
        the_last->scrubbed = false;
    }
    erase(begin(), --end());
}

void
Snapshots::getTLogsTillSnapshot(const SnapshotNum num,
                                OrderedTLogIds& out) const
{
    bool including = true; //false makes never sense
    for(const_iterator i = begin(); i != end(); ++i)
    {
        if(i->num == num)
        {
            if(including)
            {
                i->getOrderedTLogIds(out);
                return;
            }
            else
            {
                return;
            }
        }
        else
        {
            i->getOrderedTLogIds(out);
        }
    }
    LOG_ERROR("Snapshot with id " << num << " does not exist");
    throw fungi::IOException("Snapshot does not exist");
}

void
Snapshots::getTLogsAfterSnapshot(const SnapshotNum num,
                                 OrderedTLogIds& out) const
{
    for(const_reverse_iterator i = rbegin(); i !=rend();++i)
    {
        if(i->num == num)
        {
            std::reverse(out.begin(), out.end());
            return;
        }
        else
        {
            i->getReverseOrderedTLogIds(out);
        }
    }
    LOG_ERROR("Snapshot with id " << num << " does not exist");
    throw fungi::IOException("Snapshot does not exist");
}

void
Snapshots::getTLogsBetweenSnapshots(const SnapshotNum start,
                                    const SnapshotNum end_snap,
                                    OrderedTLogIds& out,
                                    IncludingEndSnapshot include_last) const
{
    if(start > end_snap)
    {
        LOG_ERROR("Start snapshot with num " << start << " is bigger than end " << end_snap);
        throw fungi::IOException("Snapshots passed are not correctly ordered");
    }

    bool start_seen = false;
    for(const_iterator i = begin(); i != end(); ++i)
    {
        if(i->num == start)
        {
            start_seen = true;
        }

        if((i->num > start)
           and (i->num < end_snap))
        {
            if(not start_seen)
            {
                LOG_ERROR("Snapshot with id " << start << " does not exist");
                throw fungi::IOException("Snapshot does not exist");
            }

            i->getOrderedTLogIds(out);
        }
        else if(i->num == end_snap)
        {
            if(not start_seen)
            {
                LOG_ERROR("Snapshot with id " << start << " does not exist");
                throw fungi::IOException("Snapshot does not exist");
            }

            if(include_last == IncludingEndSnapshot::T)
            {
                i->getOrderedTLogIds(out);
                return;
            }
            else
            {
                return;
            }
        }
    }

    LOG_ERROR("Snapshot with id " << end_snap << " does not exist");
    throw fungi::IOException("Snapshot does not exist");
}

bool
Snapshots::snip(const TLogId& tlog_id,
                const boost::optional<uint64_t>& backend_size)
{
    bool found = false;
    iterator it;

    for(it = begin(); it != end() and not found; ++it)
    {
        LOG_INFO("Checking snapshot " << it->getName() << " for tlog " << tlog_id);

        found = it->snip(tlog_id,
                         backend_size);
    }

    if(it != end())
    {
        LOG_INFO("Snipping at snapshot " << it->getName()
                 << " tlog " << tlog_id);

        for(const_iterator it2 = it; it2 != end(); ++it2)
        {
            LOG_INFO("Snipping away " << it2->getName());
        }

        erase(it, end());
    }
    return found;
}

void
Snapshots::deleteTLogsAndSnapshotsAfterSnapshot(const SnapshotNum num)
{
    iterator i = find_or_throw_(num);
    erase(++i, end());
}

void
Snapshots::getAllSnapshots(std::vector<SnapshotNum>& out) const
{
    for(const_iterator i = begin(); i != end(); ++i)
    {
        out.push_back(i->num);
    }
}

namespace
{

bool
already_replaced(const std::vector<TLog>& vec,
                 const std::list<TLog>& list)
{
    size_t i = 0;

    if (vec.size() and vec.size() == list.size())
    {
        for (const auto& t : list)
        {
            if (t != vec[i++])
            {
                return false;
            }
        }

        return true;
    }

    return false;
}

}

void
Snapshots::replace(const OrderedTLogIds& in,
                   const std::vector<TLog>& out,
                   const SnapshotNum num)
{
    for (const auto& t : out)
    {
        VERIFY(t.writtenToBackend());
    }

    iterator i = find_or_throw_(num);

    if (already_replaced(out,
                         *i))
    {
        LOG_INFO("Snapshot " << i->getUUID() << " (" << i->getName() <<
                 "): already replaced");
    }
    else
    {
        i->replace(in, out);
    }
}

// start_snap (if specified) is *exclusive*, end_snap (if specified) is *inclusive*, IOW:
// ( start_snap, end_snap ]
void
Snapshots::getSnapshotScrubbingWork(const boost::optional<SnapshotName>& start_snap,
                                    const boost::optional<SnapshotName>& end_snap,
                                    SnapshotWork& out) const
{
    LOG_TRACE("start: " << start_snap << ", end: " << end_snap);

    // Iterating twice is of course less efficient but the code becomes much easier to
    // digest (at least for yours truly).
    // If this should become a bottleneck there are too many snapshots anyway.

    boost::optional<const_iterator> sit = boost::none;
    boost::optional<const_iterator> eit = boost::none;

    if (start_snap or end_snap)
    {
        for (auto it = begin(); it != end(); ++it)
        {
            if (start_snap and it->name == *start_snap)
            {
                VERIFY(sit == boost::none);
                if (eit)
                {
                    LOG_ERROR("Requested end snapshot " << *end_snap <<
                              " precedes requested start snapshot " << *start_snap);
                    throw fungi::IOException("Requested end snapshot precedes requested start snapshot");
                }

                auto tmp = it;
                sit = ++tmp;
            }

            if (end_snap and it->name == *end_snap)
            {
                VERIFY(eit == boost::none);
                auto tmp = it;
                eit = ++tmp;
            }
        }
    }

    if (not sit)
    {
        if (start_snap)
        {
            LOG_ERROR("Requested start snapshot " << *start_snap << " does not exist");
            throw fungi::IOException("Requested start snapshot does not exist",
                                     (*start_snap).c_str());
        }
    }

    if (not eit)
    {
        if (end_snap)
        {
            LOG_ERROR("Requested end snapshot " << *end_snap << " does not exist");
            throw fungi::IOException("Requested end snapshot does not exist",
                                     (*end_snap).c_str());
        }
    }

    for(const_iterator it = sit.get_value_or(begin());
        it != eit.get_value_or(end());
        ++it)
    {
        if(not it->scrubbed and
           it->writtenToBackend())
        {
            const SnapshotWorkUnit workunit(it->name);
            out.push_back(workunit);
        }
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
