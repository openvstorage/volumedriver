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
