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

#ifndef _SNAPSHOT_PERSISTOR_TOOLCUT_H_
#define _SNAPSHOT_PERSISTOR_TOOLCUT_H_

#include <string>

#include <boost/shared_ptr.hpp>

#include <boost/python/def.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/list.hpp>
#include <boost/python/object.hpp>
#include <boost/python/overloads.hpp>

#include <volumedriver/SnapshotPersistor.h>
#include <volumedriver/TLog.h>

namespace volumedriver
{

namespace python
{

class SnapshotPersistorToolCut
{
public:
    explicit SnapshotPersistorToolCut(const std::string& file);

    boost::python::list
    getSnapshots() const;

    void
    forEach(const boost::python::object& obj) const;

    volumedriver::SnapshotNum
    getSnapshotNum(const std::string& snapshot) const;

    std::string
    getSnapshotName(const volumedriver::SnapshotNum) const;

    boost::python::list
    getSnapshotsTill(const std::string& snapshotName,
                     bool including) const;

    boost::python::list
    getSnapshotsAfter(const std::string& snapshotName) const;

    boost::python::list
    getAllTLogs(bool withCurrent) const;

    boost::python::list
    getCurrentTLogs() const;

    std::string
    getParentNamespace() const;

    std::string
    getParentSnapshot() const;

    volumedriver::TLogId
    getCurrentTLog() const;

    boost::python::list
    getTLogsInSnapshot(const std::string& snapshotName) const;

    boost::python::list
    getTLogsTillSnapshot(const std::string& snapshotName) const;

    boost::python::list
    getTLogsAfterSnapshot(const std::string& snapshotName) const;

    bool
    snapshotExists(const std::string& snapshotName);

    uint64_t
    currentStored() const;

    uint64_t
    snapshotStored(const std::string& name) const;

    uint64_t
    stored() const;

    void
    deleteSnapshot(const std::string& snapshot);

    void
    trimToBackend();

    void
    setTLogWrittenToBackend(const volumedriver::TLogId&);

    bool
    isTLogWrittenToBackend(const volumedriver::TLogId&) const;

    boost::python::list
    getTLogsNotWrittenToBackend() const;

    bool
    isSnapshotScrubbed(const std::string& snapshotName) const;

    void
    setSnapshotScrubbed(const std::string& snapshotName,
                        bool scrubbed) const;

    void
    saveToFile(const std::string& file) const;

    void
    snip(const volumedriver::TLogId&);

    void
    replace(const boost::python::list& new_ones,
            const std::string& snapshot_name);

    std::string
    str() const;

    std::string
    repr() const;

    boost::python::list
    getScrubbingWork(boost::python::object start_snap = boost::python::object(),
                     boost::python::object end_snap = boost::python::object());

protected:
    boost::shared_ptr<volumedriver::SnapshotPersistor> snapshot_persistor_;
};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(GetScrubbingWorkOverloads,
                                       getScrubbingWork,
                                       0,
                                       2);

}

}

#endif // !_SNAPSHOT_PERSISTOR_TOOLCUT_H_

// Local Variables: **
// mode: c++ **
// End: **
