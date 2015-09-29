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
    TLogCutter(BackendInterface* bi,
               const boost::filesystem::path& file,
               FilePool& filepool,
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

    BackendInterface* bi_;
    const boost::filesystem::path file_;
    FilePool& filepool_;
    const uint64_t max_entries_;

    std::unique_ptr<TLogWriter> tlog_writer;
    boost::filesystem::path current_tlog_path;

    std::vector<TLog> tlogs_;
};

}

#endif  // TLOG_CUTTER_H_

// Local Variables: **
// End: **
