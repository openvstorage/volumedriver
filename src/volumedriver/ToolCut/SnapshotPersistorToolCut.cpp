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

#include <boost/python.hpp>
#include "SnapshotPersistorToolCut.h"
#include "SnapshotToolCut.h"

#include <youtils/FileUtils.h>

#include "../SnapshotManagement.h"

#include <backend-python/ConnectionInterface.h>

namespace toolcut
{

namespace bpy = boost::python;
namespace fs = boost::filesystem;
namespace vd = volumedriver;

void
SnapshotPersistorToolCut::forEach(const bpy::object& obj) const
{
    const auto& snaps = snapshot_persistor_->getSnapshots();
    snaps.for_each_snapshot([&](const vd::Snapshot& snap)
                            {
                                obj(SnapshotToolCut(snap));
                            });
}

bpy::list
SnapshotPersistorToolCut::getSnapshots() const
{
    bpy::list result;
    const auto& snaps = snapshot_persistor_->getSnapshots();
    snaps.for_each_snapshot([&](const vd::Snapshot& snap)
                            {
                                result.append(SnapshotToolCut(snap));
                            });
    return result;
}


vd::SnapshotNum
SnapshotPersistorToolCut::getSnapshotNum(const std::string& snapshot) const
{
    return snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snapshot));
}

std::string
SnapshotPersistorToolCut::getSnapshotName(const vd::SnapshotNum snapshot) const
{
    return snapshot_persistor_->getSnapshotName(snapshot);
}

bpy::list
SnapshotPersistorToolCut::getSnapshotsAfter(const std::string& snap_name) const
{
    bpy::list result;
    std::vector<vd::SnapshotNum> snapshotNums;
    vd::SnapshotNum snum = snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snap_name));

    snapshot_persistor_->getSnapshotsAfter(snum,
                                           snapshotNums);

    for (const auto& snum : snapshotNums)
    {
        result.append(snapshot_persistor_->getSnapshotName(snum));
    }

    return result;
}

uint64_t
SnapshotPersistorToolCut::currentStored() const
{
    return snapshot_persistor_->getCurrentBackendSize();
}

uint64_t
SnapshotPersistorToolCut::snapshotStored(const std::string& name) const
{
    return snapshot_persistor_->getSnapshotBackendSize(vd::SnapshotName(name));
}

uint64_t
SnapshotPersistorToolCut::stored() const
{
    return snapshot_persistor_->getTotalBackendSize();
}

bpy::list
SnapshotPersistorToolCut::getSnapshotsTill(const std::string& snap_name,
                                           bool including) const
{
    bpy::list result;
    std::vector<vd::SnapshotNum> snapshotNums;
    vd::SnapshotNum snum = snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snap_name));
    snapshot_persistor_->getSnapshotsTill(snum,
                                          snapshotNums,
                                          including);

    for (const auto& snum : snapshotNums)
    {
        result.append(snapshot_persistor_->getSnapshotName(snum));
    }
    return result;
}

bpy::list
SnapshotPersistorToolCut::getAllTLogs(bool withCurrent) const
{
    bpy::list result;
    vd::OrderedTLogNames tlogs;
    snapshot_persistor_->getAllTLogs(tlogs,
                                     withCurrent? WithCurrent::T : WithCurrent::F);

    for (const auto& tlog : tlogs)
    {
        result.append(tlog);
    }
    return result;
}

bpy::list
SnapshotPersistorToolCut::getCurrentTLogs() const
{
    bpy::list result;
    vd::OrderedTLogNames tlogs;
    snapshot_persistor_->getCurrentTLogs(tlogs);

    for (const auto& tlog : tlogs)
    {
        result.append(tlog);
    }
    return result;
}

std::string
SnapshotPersistorToolCut::getParentNamespace() const
{
    if (snapshot_persistor_->parent())
    {
        return snapshot_persistor_->parent()->nspace.str();
    }
    else
    {
        return "";
    }
}

std::string
SnapshotPersistorToolCut::getParentSnapshot() const
{
    if (snapshot_persistor_->parent())
    {
        return snapshot_persistor_->parent()->snapshot;
    }
    else
    {
        return "";
    }
}

std::string
SnapshotPersistorToolCut::getCurrentTLog() const
{
    return snapshot_persistor_->getCurrentTLog();
}

bpy::list
SnapshotPersistorToolCut::getTLogsTillSnapshot(const std::string& snap_name) const
{
    bpy::list result;
    vd::OrderedTLogNames tlogs;
    vd::SnapshotNum snum = snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snap_name));
    snapshot_persistor_->getTLogsTillSnapshot(snum,
                                              tlogs);

    for (const auto& tlog : tlogs)
    {
        result.append(tlog);
    }
    return result;
}

bpy::list
SnapshotPersistorToolCut::getTLogsInSnapshot(const std::string& snap_name) const
{
    bpy::list result;
    vd::OrderedTLogNames tlogs;
    vd::SnapshotNum snum = snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snap_name));
    snapshot_persistor_->getTLogsInSnapshot(snum,
                                              tlogs);

    for (const auto& tlog : tlogs)
    {
        result.append(tlog);
    }
    return result;
}

bpy::list
SnapshotPersistorToolCut::getTLogsAfterSnapshot(const std::string& snap_name) const
{
    bpy::list result;
    vd::OrderedTLogNames tlogs;
    vd::SnapshotNum snum = snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snap_name));
    snapshot_persistor_->getTLogsAfterSnapshot(snum,
                                              tlogs);

    for (const auto& tlog : tlogs)
    {
        result.append(tlog);
    }
    return result;

}

bool
SnapshotPersistorToolCut::snapshotExists(const std::string& snap_name)
{
    return snapshot_persistor_->snapshotExists(vd::SnapshotName(snap_name));
}

void
SnapshotPersistorToolCut::deleteSnapshot(const std::string& snap_name)
{
    vd::SnapshotNum snum = snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snap_name));
    snapshot_persistor_->deleteSnapshot(snum);
}

void
SnapshotPersistorToolCut::trimToBackend()
{
    snapshot_persistor_->trimToBackend();
}

void
SnapshotPersistorToolCut::setTLogWrittenToBackend(const std::string& tlog)
{
    const auto id(boost::lexical_cast<vd::TLogId>(tlog));
    snapshot_persistor_->setTLogWrittenToBackend(id);
}

bool
SnapshotPersistorToolCut::isTLogWrittenToBackend(const std::string& tlog) const
{
    const auto id(boost::lexical_cast<vd::TLogId>(tlog));
    return snapshot_persistor_->isTLogWrittenToBackend(id);
}

bpy::list
SnapshotPersistorToolCut::getTLogsNotWrittenToBackend() const
{
    bpy::list result;
    vd::OrderedTLogNames tlogs;
    snapshot_persistor_->getTLogsNotWrittenToBackend(tlogs);

    for (const auto& tlog : tlogs)
    {
        result.append(tlog);
    }
    return result;
}

bool
SnapshotPersistorToolCut::isSnapshotScrubbed(const std::string& snap_name) const
{
    vd::SnapshotNum snum = snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snap_name));
    return snapshot_persistor_->getSnapshotScrubbed(snum, false);
}

void
SnapshotPersistorToolCut::setSnapshotScrubbed(const std::string& snap_name,
                                              bool scrubbed) const
{
    vd::SnapshotNum snum = snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snap_name));
    snapshot_persistor_->setSnapshotScrubbed(snum, scrubbed);
}

SnapshotPersistorToolCut::SnapshotPersistorToolCut(bpy::object& backend,
                                                   const std::string& nspace)
{
    fs::path p(vd::FileUtils::create_temp_file_in_temp_dir(std::string("snapshots")));
    ALWAYS_CLEANUP_FILE(p);

    backend.attr("read")(nspace,
                         p.string(),
                         vd::snapshotFilename(),
                         true);
    snapshot_persistor_.reset(new vd::SnapshotPersistor(p));
}


SnapshotPersistorToolCut::SnapshotPersistorToolCut(const std::string& p)
{
    snapshot_persistor_.reset(new vd::SnapshotPersistor(p));
}


void
SnapshotPersistorToolCut::saveToFile(const std::string& p) const
{
    snapshot_persistor_->saveToFile(p, SyncAndRename::T);
}

void
SnapshotPersistorToolCut::snip(const std::string& tlogname)
{
    snapshot_persistor_->snip(tlogname,
                              boost::none);
}

void
SnapshotPersistorToolCut::replace(const bpy::list& new_ones,
                                  const std::string& snap_name)
{
    vd::SnapshotNum snap_num = snapshot_persistor_->getSnapshotNum(vd::SnapshotName(snap_name));
    vd::OrderedTLogNames olden_ones;
    snapshot_persistor_->getTLogsInSnapshot(snap_num,
                                            olden_ones);
    uint64_t len_new_ones = len(new_ones);
    std::vector<vd::TLog> news;
    news.reserve(len_new_ones);

    for(unsigned i = 0; i < len_new_ones; ++i)
    {
        news.emplace_back(bpy::extract<vd::TLog>(new_ones[i]));
    }

    snapshot_persistor_->replace(olden_ones,
                                 news,
                                 snap_num);
}

bpy::list
SnapshotPersistorToolCut::getScrubbingWork(bpy::object start_snap,
                                           bpy::object end_snap)
{
    boost::optional<vd::SnapshotName> ssnap;
    if (start_snap != bpy::object())
    {
        ssnap = bpy::extract<vd::SnapshotName>(start_snap);
    }

    boost::optional<vd::SnapshotName> esnap;
    if (end_snap != bpy::object())
    {
        esnap = bpy::extract<vd::SnapshotName>(end_snap);
    }

    // Keep this in line with VolumeGetWork XMLRPC call so client sees the same interface
    vd::SnapshotWork work;

    snapshot_persistor_->getSnapshotScrubbingWork(ssnap,
                                                  esnap,
                                                  work);

    bpy::list snapshots;

    for (vd::SnapshotWork::const_iterator i = work.begin();
         i != work.end();
         ++i)
    {
        snapshots.append(*i);
    }

    return snapshots;
}

std::string
SnapshotPersistorToolCut::str() const
{
    std::stringstream ss;
    ss << "snapshots:" << std::endl;

    const auto& snaps = snapshot_persistor_->getSnapshots();
    snaps.for_each_snapshot([&](const vd::Snapshot& snap)
                            {
                                ss << "\tname: " << snap.getName() << std::endl;
                                ss << "\tdate: " << snap.getDate() << std::endl;
                                ss << "\tbackendsize: "  << snap.backend_size() <<
                                    std::endl;
                                ss << "\tuuid: "  << (snap.hasUUIDSpecified() ?
                                                      snap.getUUID().str() :
                                                      "<not specified>") << std::endl;
                                ss << "\ttlogs:" << std::endl;

                                snap.for_each_tlog([&](const vd::TLog& tlog)
                                                   {
                                                       ss << "\t\t" << tlog.getName() <<
                                                           std::endl;
                                                   });
                            });

    ss << "current:\n";
    const vd::OrderedTLogNames cur(snapshot_persistor_->getCurrentTLogs());
    for (const auto& tlog : cur)
    {
        ss << "\t\t" << tlog << std::endl;
    }

    return ss.str();
}

std::string
SnapshotPersistorToolCut::repr() const
{
    return std::string("< SnapshotPersistor \n") + str() + "\n>";
}

}

// Local Variables: **
// mode: c++ **
// End: **
