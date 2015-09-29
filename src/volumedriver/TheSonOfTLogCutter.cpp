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

#include "TheSonOfTLogCutter.h"
#include <youtils/UUID.h>
#include "TLog.h"

namespace volumedriver

{

TheSonOfTLogCutter::TheSonOfTLogCutter(BackendInterface* bi,
                                       const fs::path& file,
                                       FilePool& filepool,
                                       std::vector<std::string>& tlog_names,
                                       uint64_t max_entries)
    : bi_(bi),
      file_(file),
      filepool_(filepool),
      tlog_names_(tlog_names),
      max_entries_(max_entries)
{
}

void
TheSonOfTLogCutter::makeNewTLog()
{
    LOG_DEBUG("Creating an new tlog");
    UUID new_id;
    const std::string new_tlog =  TLog::getName(new_id);
    current_tlog_path = filepool_.newFile(new_tlog);
    tlog_writer.reset(new TLogWriter(current_tlog_path));
    LOG_DEBUG("Created " << new_tlog << " at " << current_tlog_path);
    tlog_names_.push_back("relocation_" + new_tlog);
}

void
TheSonOfTLogCutter::writeTLogToBackend()
{

    VERIFY(tlog_writer.get());
    VERIFY(!tlog_names_.empty());

    LOG_DEBUG("Writing " << current_tlog_path << " To the backend as " << tlog_names_.back());
    tlog_writer->sync();
    CheckSum check = tlog_writer->getCheckSum();
    bi_->write(current_tlog_path,
               tlog_names_.back(),
               OverwriteObject::F,
               &check);
}

void
TheSonOfTLogCutter::operator()()
{
    uint64_t entries_written = 0;
    makeNewTLog();
    TLogReader tlog_reader(file_);
    const Entry* e  = 0;
    while((e = tlog_reader.nextLocation()))
    {
        if(entries_written >= max_entries_ and
           (entries_written % 2 == 0))
        {
            writeTLogToBackend();
            makeNewTLog();
            entries_written = 0;
        }
        tlog_writer->add(e->clusterAddress(),
                                     e->clusterLocationAndHash());
        entries_written++;
    }

    writeTLogToBackend();

}
}

// Local Variables: **
// End: **
