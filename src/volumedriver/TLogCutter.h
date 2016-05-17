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

#ifndef TLOG_CUTTER_H_
#define TLOG_CUTTER_H_

#include "FilePool.h"
#include "TLogReader.h"
#include "TLogWriter.h"

#include <vector>
#include <string>

#include <boost/filesystem.hpp>

#include <youtils/FileUtils.h>
#include <youtils/Logging.h>

#include <backend/BackendInterface.h>

namespace volumedriver
{

class TLog;

class TLogCutter
{
public:
    TLogCutter(BackendInterface&,
               const boost::filesystem::path& file,
               FilePool&,
               const ClusterSize,
               uint64_t max_entries = 4194304);

    ~TLogCutter() = default;

    TLogCutter(const TLogCutter&) = delete;

    TLogCutter&
    operator=(const TLogCutter&) = delete;

    std::vector<TLog>
    operator()();

private:
    DECLARE_LOGGER("TLogCutter");

    void
    makeNewTLog();

    void
    writeTLogToBackend();

    BackendInterface& bi_;
    const boost::filesystem::path file_;
    FilePool& filepool_;
    const uint64_t max_entries_;
    const ClusterSize cluster_size_;

    std::unique_ptr<TLogWriter> tlog_writer;
    boost::filesystem::path current_tlog_path;

    std::vector<TLog> tlogs_;
};

}

#endif  // TLOG_CUTTER_H_

// Local Variables: **
// End: **
