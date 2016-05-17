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

#include "SnapshotPersistor.h"
#include "TracePoints_tp.h"
#include "VolumeDriverError.h"

#include <cerrno>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <limits>

#include <boost/filesystem/fstream.hpp>
#include <boost/archive/archive_exception.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/optional.hpp>

#include <youtils/Assert.h>
#include <youtils/ScopeExit.h>

namespace volumedriver
{

namespace yt = youtils;
namespace be = backend;

const char* SnapshotPersistor::nvp_name = "snapshots";

SnapshotPersistor::SnapshotPersistor(const MaybeParentConfig& p)
    : parent_(p)
{
    newTLog();
}

SnapshotPersistor::SnapshotPersistor(const fs::path& path)
{
    LOG_TRACE("path: " << path);

    if(fs::exists(path))
    {
        try
        {
            fs::ifstream ifs(path);
            boost::archive::xml_iarchive ia(ifs);
            ia >> boost::serialization::make_nvp(nvp_name,
                                                 *this);
        }
        CATCH_STD_ALL_EWHAT({
                VolumeDriverError::report(events::VolumeDriverErrorCode::ReadSnapshots,
                                          EWHAT);

                LOG_ERROR("Caught exception while deserializing " << path << ": " <<
                          EWHAT);

#ifndef NDEBUG
                fs::copy_file(path, FileUtils::temp_path() / "faulty_xml_archive");
#endif
                throw;
            });
    }
    else
    {
        std::stringstream ss;
        ss << "snapshots file " << path << " does not exist";
        LOG_ERROR(ss.str());
        VolumeDriverError::report(events::VolumeDriverErrorCode::ReadSnapshots,
                                  ss.str());
        throw fungi::IOException(ss.str());
    }
}

SnapshotPersistor::SnapshotPersistor(std::istream& istr)
{
    LOG_TRACE("reading from istream");

    try
    {
        boost::archive::xml_iarchive ia(istr);
        ia >> boost::serialization::make_nvp(nvp_name, *this);
    }
    catch(std::exception& e)
    {
        LOG_ERROR("Could not read snapshots stream, " << e.what());
        throw;
    }
    catch(...)
    {
        LOG_ERROR("Could not read snapshots stream, Unknown Exception");
        throw;
    }
}

SnapshotPersistor::SnapshotPersistor(BackendInterfacePtr& bi)
{
    LOG_TRACE("filling from backend namespace " << bi->getNS());
    bi->fillObject(*this,
                   snapshotFilename(),
                   nvp_name,
                   InsistOnLatestVersion::T);
}

std::unique_ptr<SnapshotPersistor>
SnapshotPersistor::parentSnapshotPersistor(BackendInterfacePtr& bi) const
{
    if(parent_)
    {
        return bi->cloneWithNewNamespace(parent_->nspace)->getIstreamableObject<SnapshotPersistor>(snapshotFilename(),
                                                                                              InsistOnLatestVersion::T);
    }
    else
    {
        return std::unique_ptr<SnapshotPersistor>();
    }
}

std::string
SnapshotPersistor::getFormatedTime()
{
    // "YYYY-MM-DDThh:mm:ss" is xml dateTime
    const static char* templ = "%FT%T";
    time_t t = time(0);
    if(t== -1)
    {
        LOG_ERROR("Could not get time");
        return std::string("2000-09-23T00:00:00");
    }
    struct tm* tm = localtime(&t);
    if(tm == 0)
    {
        LOG_ERROR("Could not get time");
        return std::string("2000-09-23T00:00:00");
    }
    char result[20];
    size_t res = strftime(result,20,templ,tm);
    if(res==0)
    {
        LOG_ERROR("Could not get time");
        return std::string("2000-09-23T00:00:00");
    }
    return std::string(result);
}

bool
SnapshotPersistor::isSnapshotInBackend(const SnapshotNum num) const
{
    return snapshots.getSnapshot(num).inBackend();
}

void
SnapshotPersistor::saveToFile(const fs::path& filename,
                                  const SyncAndRename sync_and_rename) const
{
    tracepoint(openvstorage_volumedriver,
               snapshots_persist_start,
               filename.string().c_str(),
               sync_and_rename == SyncAndRename::T);

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         tracepoint(openvstorage_volumedriver,
                                                    snapshots_persist_end,
                                                    filename.string().c_str(),
                                                    sync_and_rename == SyncAndRename::T,
                                                    std::uncaught_exception());
                                     }));
    try
    {
        if (T(sync_and_rename))
        {
            fs::path tmp;
            tmp = FileUtils::create_temp_file(filename);
            ALWAYS_CLEANUP_FILE(tmp);
            saveToFileSimple_(tmp);
            {
                yt::FileDescriptor justToSync(tmp,
                                              yt::FDMode::Write);
            }

            fs::rename(tmp, filename);
        }
        else
        {
            saveToFileSimple_(filename);
        }
    }
    CATCH_STD_ALL_LOGLEVEL_ADDERROR_RETHROW("Failed to persist snapshots",
                                            ERROR,
                                            events::VolumeDriverErrorCode::WriteSnapshots);
}

void
SnapshotPersistor::saveToFileSimple_(const fs::path& filename) const
{
    yt::Serialization::serializeNVPAndFlush<boost::archive::xml_oarchive>(filename,
                                                                          "snapshots",
                                                                          *this);
}

void
SnapshotPersistor::newTLog()
{
    current.emplace_back(TLog());
    LOG_INFO("Starting new TLog " << current.back().id());
}

void
SnapshotPersistor::setTLogWrittenToBackend(const TLogId& tlogid)
{
    // The "snapshots before current" order needs to be maintained as the
    // consistency check in TLogs::setTLogWrittenToBackend() relies on it.
    if(not snapshots.setTLogWrittenToBackend(tlogid))
    {
        if (not current.setTLogWrittenToBackend(tlogid))
        {
            LOG_WARN("Couldn't find tlog " << tlogid <<
                     " probably snapshot restore. These messages should not persist!");
        }
    }
}

bool
SnapshotPersistor::isTLogWrittenToBackend(const TLogId& tlogid) const
{
    boost::tribool b = current.isTLogWrittenToBackend(tlogid);
    if(b or not b)
    {
        return bool(b);
    }
    else
    {
        return snapshots.isTLogWrittenToBackend(tlogid);
    }
}

void
SnapshotPersistor::getCurrentTLogsWrittenToBackend(OrderedTLogIds& out) const
{

    for(const TLog& t: current)
    {
        if(isTLogWrittenToBackend(t.id()))
        {
            out.push_back(t.id());
        }
    }
}

void
SnapshotPersistor::getTLogsNotWrittenToBackend(OrderedTLogIds& out) const
{
    OrderedTLogIds tmp;
    getAllTLogs(tmp, WithCurrent::T);
    bool no_non_written_tlog_seen = true;
    for(size_t i = 0; i < tmp.size(); i++)
    {
        if(isTLogWrittenToBackend(tmp[i]))
        {
            VERIFY(no_non_written_tlog_seen);
        }
        else
        {
            no_non_written_tlog_seen = false;
            out.push_back(tmp[i]);
        }
    }
}

void
SnapshotPersistor::getTLogsWrittenToBackend(OrderedTLogIds& out) const
{
    OrderedTLogIds tmp;
    getAllTLogs(tmp, WithCurrent::T);
    bool no_non_written_tlog_seen = true;
    for(size_t i = 0; i < tmp.size(); i++)
    {
        if(isTLogWrittenToBackend(tmp[i]))
        {
            VERIFY(no_non_written_tlog_seen);
            out.push_back(tmp[i]);
        }
        else
        {
            no_non_written_tlog_seen = false;
        }
    }
}

void
SnapshotPersistor::snapshot(const SnapshotName& name,
                            const SnapshotMetaData& metadata,
                            const yt::UUID& uuid,
                            const bool create_scrubbed)
{
    if(snapshotExists(name))
    {
        LOG_ERROR("Couldn't create snapshot " << name <<
                  " - a snapshot of that name already exists");
        throw SnapshotNameAlreadyExists(name.c_str());
    }

    if (metadata.size() > max_snapshot_metadata_size)
    {
        LOG_ERROR("Refusing to create snapshot " << name <<
                  " as the attached metadata exceeds the limit of " <<
                  max_snapshot_metadata_size <<
                  "bytes");
        throw SnapshotPersistorException("Metadata size exceeds limit",
                                         name.c_str());
    }

    const Snapshot s(snapshots.getNextSnapshotNum(),
                     name,
                     current,
                     metadata,
                     uuid,
                     create_scrubbed);

    snapshots.push_back(s);
    current.clear();
    newTLog();
}

bool
SnapshotPersistor::checkSnapshotUUID(const SnapshotName& snapshotName,
                                     const volumedriver::UUID& uuid) const
{
    return snapshots.checkSnapshotUUID(snapshotName,
                                       uuid);
}

bool
SnapshotPersistor::getTLogsInSnapshot(const SnapshotNum num,
                                      OrderedTLogIds& outTLogs) const
{
    return snapshots.getTLogsInSnapshot(num,
                                        outTLogs);
}

void
SnapshotPersistor::getCurrentTLogs(OrderedTLogIds& outTLogs) const
{
    current.getTLogIds(outTLogs);
}

TLogId
SnapshotPersistor::getCurrentTLog() const
{
    VERIFY(!current.empty());
    return current.back().id();
}

bool
SnapshotPersistor::snapshotExists(const SnapshotName& name) const
{
    return snapshots.snapshotExists(name);
}

bool
SnapshotPersistor::snapshotExists(SnapshotNum num) const
{
    return snapshots.snapshotExists(num);
}

SnapshotNum
SnapshotPersistor::getSnapshotNum(const SnapshotName& name) const
{
    return getSnapshot(name).snapshotNumber();
}

const Snapshot&
SnapshotPersistor::getSnapshot(const SnapshotName& name) const
{
    return snapshots.getSnapshot(name);
}

SnapshotName
SnapshotPersistor::getSnapshotName(SnapshotNum num) const
{
    return snapshots.getSnapshot(num).getName();
}

const yt::UUID&
SnapshotPersistor::getUUID(SnapshotNum num) const
{
    return snapshots.getSnapshot(num).getUUID();
}

bool
SnapshotPersistor::hasSnapshotWithUUID(const yt::UUID& uuid) const
{
    return snapshots.hasSnapshotWithUUID(uuid);
}

void
SnapshotPersistor::getSnapshotsTill(SnapshotNum num,
                                    std::vector<SnapshotNum>& out,
                                    bool including) const
{
    snapshots.getSnapshotsTill(num,
                               out,
                               including);
}

void
SnapshotPersistor::getSnapshotsAfter(SnapshotNum num,
                                     std::vector<SnapshotNum>& out) const
{
    snapshots.getSnapshotsAfter(num,
                                out);
}

void
SnapshotPersistor::deleteSnapshot(SnapshotNum num)
{
    snapshots.deleteSnapshot(num,
                             current);
}

void
SnapshotPersistor::deleteAllButLastSnapshot()
{
    snapshots.deleteAllButLastSnapshot();
}

void
SnapshotPersistor::getTLogsTillSnapshot(const SnapshotName& name,
                                        OrderedTLogIds& out) const
{
    getTLogsTillSnapshot(getSnapshotNum(name),
                         out);
}

void
SnapshotPersistor::getTLogsTillSnapshot(SnapshotNum num,
                                        OrderedTLogIds& out) const
{
    snapshots.getTLogsTillSnapshot(num,
                                   out);
}

void
SnapshotPersistor::getTLogsAfterSnapshot(SnapshotNum num,
                                         OrderedTLogIds& out) const
{
    snapshots.getTLogsAfterSnapshot(num,
                                    out);
    current.getTLogIds(out);
}

void
SnapshotPersistor::getTLogsBetweenSnapshots(const SnapshotNum start,
                                            const SnapshotNum end,
                                            OrderedTLogIds& out,
                                            IncludingEndSnapshot including) const
{
    snapshots.getTLogsBetweenSnapshots(start,
                                       end,
                                       out,
                                       including);
}

void
SnapshotPersistor::deleteTLogsAndSnapshotsAfterSnapshot(SnapshotNum num)
{
    snapshots.deleteTLogsAndSnapshotsAfterSnapshot(num);
    current.clear();
    newTLog();
}

bool
SnapshotPersistor::snip(const TLogId& tlog_id,
                        const boost::optional<uint64_t>& backend_size)
{
    bool found = snapshots.snip(tlog_id,
                                backend_size);
    if(found)
    {
        current.clear();
        current = snapshots.back();
        snapshots.pop_back();
        return true;
    }
    else
    {
        return current.snip(tlog_id,
                            backend_size);
    }
}

bool
SnapshotPersistor::tlogReferenced(const TLogId& tlog_id)
{
    return snapshots.tlogReferenced(tlog_id) or
        current.tlogReferenced(tlog_id);
}

void
SnapshotPersistor::getAllSnapshots(std::vector<SnapshotNum>& vec) const
{
    snapshots.getAllSnapshots(vec);
}

void
SnapshotPersistor::getAllTLogs(OrderedTLogIds& vec,
                               const WithCurrent with_current) const
{
    std::vector<SnapshotNum> snaps;
    getAllSnapshots(snaps);
    std::sort(snaps.begin(),snaps.end());
    for(unsigned i = 0; i < snaps.size();i++)
    {
        getTLogsInSnapshot(snaps[i],
                           vec);

    }
    if(with_current == WithCurrent::T)
    {
        getCurrentTLogs(vec);
    }
}

ScrubId
SnapshotPersistor::replace(const OrderedTLogIds& in,
                           const std::vector<TLog>& out,
                           const SnapshotNum num){
    snapshots.replace(in,
                      out,
                      num);
    return new_scrub_id();
}

bool
SnapshotPersistor::getSnapshotScrubbed(SnapshotNum num,
                                       bool nothrow) const
{
    if(not snapshotExists(num))
    {
        if(nothrow)
        {
            return true;
        }
        else
        {
            LOG_ERROR("Snapshot with id " << num << " does not exist");
            throw fungi::IOException("Snapshot does not exist");
        }
    }

    return snapshots.getSnapshot(num).scrubbed;
}

void
SnapshotPersistor::setSnapshotScrubbed(SnapshotNum num,
                                       bool scrubbed)
{
    snapshots.find_or_throw_(num)->scrubbed = scrubbed;
}

void
SnapshotPersistor::getSnapshotScrubbingWork(const boost::optional<SnapshotName>& start_snap,
                                            const boost::optional<SnapshotName>& end_snap,
                                            SnapshotWork& out) const
{
    snapshots.getSnapshotScrubbingWork(start_snap,
                                       end_snap,
                                       out);
}

// DOES NOT SET CURRENT SIZE!!
OrderedTLogIds
SnapshotPersistor::getTLogsOnBackendSinceLastCork(const boost::optional<yt::UUID>& cork,
                                                  const boost::optional<yt::UUID>& implicit_start_cork) const
{
    VERIFY((parent_ and implicit_start_cork != boost::none) or
           (not parent_ and implicit_start_cork == boost::none));

    OrderedTLogIds tlog_names;

    auto done([&tlog_names]() -> OrderedTLogIds
              {
                  std::reverse(tlog_names.begin(), tlog_names.end());
                  return tlog_names;
              });

    if(current.getReversedTLogsOnBackendSinceLastCork(cork,
                                                      tlog_names))
    {
        return done();
    }

    for(Snapshots::const_reverse_iterator s = snapshots.rbegin();
        s != snapshots.rend();
        ++s)
    {
        if(s->getReversedTLogsOnBackendSinceLastCork(cork, tlog_names))
        {
            return done();
        }
    }

    if (cork == implicit_start_cork)
    {
        return done();
    }

    LOG_ERROR("Could not find cork " << cork << " in the snapshots.xml");
    throw CorkNotFoundException("");
}

boost::optional<yt::UUID>
SnapshotPersistor::lastCork() const
{
    for(TLogs::const_reverse_iterator it = current.rbegin(); it != current.rend(); ++it)
    {
        if(it->writtenToBackend())
        {
            return it->id().t;
        }
    }

    for(Snapshots::const_reverse_iterator it = snapshots.rbegin(); it != snapshots.rend(); ++it)
    {
        if(it->writtenToBackend())
        {
            return it->getCork();
        }
    }
    //no current, no snapshots... return the parent cork, which should be the nullUUID in case of no parent
    //might fail in case of an upgraded persistor from version 0

    //    if(not hasParent())
    return boost::none;
}

void
SnapshotPersistor::trimToBackend()
{
    bool ready = false;

    for(Snapshots::iterator i = snapshots.begin(); i != snapshots.end(); i++)
    {
        LOG_TRACE(i->getName() << ": in backend: " << i->inBackend());
        if (not i->inBackend())
        {
            current = i->tlogsOnDss();
            snapshots.erase(i, snapshots.end());
            ready = true;
            break;
        }
    }

    if (not ready)
    {
        current = current.tlogsOnDss();
        ready = true;
    }

    newTLog();
}

uint64_t
SnapshotPersistor::getSnapshotBackendSize(const SnapshotName& name) const
{
    return getSnapshot(name).backend_size();
}

uint64_t
SnapshotPersistor::getCurrentBackendSize() const
{
    return current.backend_size();
}

uint64_t
SnapshotPersistor::getTotalBackendSize() const
{
    return snapshots.getTotalBackendSize() + getCurrentBackendSize();
}

void
SnapshotPersistor::addCurrentBackendSize(uint64_t s)
{
    VERIFY(not current.empty());
    current.back().add_to_backend_size(s);
}

namespace
{
struct WrittenToBackendVerifier
{
    explicit WrittenToBackendVerifier(bool& unwritten_tlog_seen)
        : unwritten_tlog_seen_(unwritten_tlog_seen)
    {}

    void
    operator()(const TLog& tlog) const
    {
        if (tlog.writtenToBackend())
        {
            if (unwritten_tlog_seen_)
            {
                const std::string
                    msg("TLog " +
                        tlog.getName() +
                        " marked as written to backend but a previous TLog was not");
                LOG_ERROR(msg);
                throw fungi::IOException(msg.c_str());
            }
        }
        else
        {
            unwritten_tlog_seen_ = true;
        }
    }

    bool& unwritten_tlog_seen_;

    DECLARE_LOGGER("TLogWrittenToBackendVerifier");
};

}

void
SnapshotPersistor::verifySanity_() const
{
    bool unwritten_tlog_seen = false;
    WrittenToBackendVerifier v(unwritten_tlog_seen);

    for (const auto& snap : snapshots)
    {
        std::for_each(snap.begin(),
                      snap.end(),
                      v);
    }

    std::for_each(current.begin(),
                  current.end(),
                  v);
}

bool
SnapshotPersistor::hasUUIDSpecified() const
{
    return snapshots.hasUUIDSpecified();
}

uint64_t
SnapshotPersistor::getBackendSize(const SnapshotName& end_snapshot,
                                  const boost::optional<SnapshotName>& start_snapshot) const
{
    return snapshots.getBackendSize(end_snapshot,
                                    start_snapshot);
}

const yt::UUID&
SnapshotPersistor::getSnapshotCork(const SnapshotName& name) const
{
    return getSnapshot(name).getCork();
}

}

// Local Variables: **
// mode: c++ **
// End: **
