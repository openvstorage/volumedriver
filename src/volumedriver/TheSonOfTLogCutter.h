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

#ifndef THESONOFTLOGCUTTER_H
#define THESONOFTLOGCUTTER_H

#include "backend/BackendInterface.h"
#include "FilePool.h"
#include "TLogReader.h"
#include "TLogWriter.h"
#include "youtils/FileUtils.h"
#include <vector>
#include <string>
#include <youtils/Logging.h>
namespace volumedriver
{
class TheSonOfTLogCutter
{
public:
    TheSonOfTLogCutter(BackendInterface* bi,
               const fs::path& file,
               FilePool& filepool,
               std::vector<std::string>& tlog_names,
               uint64_t max_entries = 4194304);

    TheSonOfTLogCutter(const TheSonOfTLogCutter&) = delete;
    TheSonOfTLogCutter& operator=(const TheSonOfTLogCutter&) = delete;

    void
    operator()();

    DECLARE_LOGGER("TheSonOfTLogCutter");

private:

    void
    makeNewTLog();

    void
    writeTLogToBackend();


    BackendInterface* bi_;
    const fs::path file_;
    FilePool& filepool_;
    std::vector<std::string>& tlog_names_;
    const uint64_t max_entries_;

    std::unique_ptr<TLogWriter> tlog_writer;
    fs::path current_tlog_path;

};


}

#endif // THESONOFTLOGCUTTER_H

// Local Variables: **
// End: **
