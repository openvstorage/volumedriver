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

#include "TLog.h"
#include "TLogCutter.h"

#include <youtils/UUID.h>

namespace volumedriver

{

TLogCutter::TLogCutter(BackendInterface& bi,
                       const fs::path& file,
                       FilePool& filepool,
                       const ClusterSize cluster_size,
                       uint64_t max_entries)
    : bi_(bi)
    , file_(file)
    , filepool_(filepool)
    , max_entries_(max_entries)
    , cluster_size_(cluster_size)
{}

void
TLogCutter::makeNewTLog()
{
    LOG_DEBUG("Creating an new tlog");
    TLog tlog;

    current_tlog_path = filepool_.newFile(tlog.getName());
    tlog_writer.reset(new TLogWriter(current_tlog_path));
    LOG_DEBUG("Created " << tlog.getName() << " at " << current_tlog_path);
    tlogs_.push_back(tlog);
}

void
TLogCutter::writeTLogToBackend()
{

    VERIFY(tlog_writer.get());
    VERIFY(!tlogs_.empty());

    LOG_DEBUG("Writing " << current_tlog_path << " To the backend as " <<
              tlogs_.back().getName());
    tlog_writer->sync();
    CheckSum check = tlog_writer->getCheckSum();

    // work around ALBA uploads timing out but eventually succeeding in the
    // background, leading to overwrite on retry.
    TODO("AR: use OverwriteObject::F instead");
    VERIFY(not bi_.objectExists(tlogs_.back().getName()));

    bi_.write(current_tlog_path,
              tlogs_.back().getName(),
              OverwriteObject::F,
              &check);
    tlogs_.back().writtenToBackend(true);
}

std::vector<TLog>
TLogCutter::operator()()
{
    uint64_t entries_written = 0;
    makeNewTLog();
    TLogReader tlog_reader(file_);
    const Entry* e  = 0;
    SCONumber prev_sco_num = 0;

    while((e = tlog_reader.nextLocation()))
    {
        const SCONumber current_sco_num = e->clusterLocation().number();

        if(entries_written >= max_entries_ and
           prev_sco_num != current_sco_num)
        {
            writeTLogToBackend();
            makeNewTLog();
            entries_written = 0;

        }
        tlog_writer->add(e->clusterAddress(),
                         e->clusterLocationAndHash());
        tlogs_.back().add_to_backend_size(cluster_size_);
        entries_written++;
        prev_sco_num = current_sco_num;
    }

    writeTLogToBackend();

    return tlogs_;
}

}

// Local Variables: **
// End: **
