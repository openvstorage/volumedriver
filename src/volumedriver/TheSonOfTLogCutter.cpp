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

#include "TheSonOfTLogCutter.h"
#include "TLog.h"
#include "TLogId.h"

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
    LOG_DEBUG("Creating a new tlog");
    TLogId new_id;
    const auto new_tlog(boost::lexical_cast<std::string>(new_id));
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

    // work around ALBA uploads timing out but eventually succeeding in the
    // background, leading to overwrite on retry.
    TODO("AR: use OverwriteObject::F instead");
    VERIFY(not bi_->objectExists(tlog_names_.back()));
    bi_->write(current_tlog_path,
               tlog_names_.back(),
               OverwriteObject::T,
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
