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

#include "TLogReader.h"
#include "Entry.h"
#include "youtils/FileUtils.h"

#include "VolumeDriverError.h"
#include "TLogWriter.h"

namespace volumedriver
{


TLogReader::TLogReader(const fs::path& TLogPath,
                       const std::string& TLogName,
                       BackendInterfacePtr bi,
                       uint32_t cache_size)
    : OneFileTLogReader(TLogPath,
                        TLogName,
                        std::move(bi))
    , buf_pos(0)
    , entries(cache_size)
    , max_pos(0)
    , buf_size(cache_size * Entry::getDataSize())
{
}


TLogReader::TLogReader(const fs::path& path,
                       uint32_t cache_size)
    : OneFileTLogReader(path)
    , buf_pos(0)
    , entries(cache_size)
    , max_pos(0)
    , buf_size(cache_size * Entry::getDataSize())
{}

bool
TLogReader::maybe_refresh_buffer()
{
    if(buf_pos == max_pos)
    {
        try
        {
            buf_pos = 0;
            max_pos = file_->read(&entries[0], buf_size) ;
            VERIFY(max_pos % Entry::getDataSize() == 0);
            max_pos /= Entry::getDataSize();
            return (max_pos != 0);
        }
        CATCH_STD_ALL_EWHAT({
                VolumeDriverError::report(events::VolumeDriverErrorCode::ReadTLog,
                                          EWHAT);
                throw;
            });
    }
    else
    {
        return true;
    }
}

const Entry*
TLogReader::nextAny()
{
    VERIFY(max_pos >= buf_pos);
    VERIFY(file_.get());
    if(maybe_refresh_buffer())
    {
        Entry* e = &entries[0] + buf_pos++;
        // VERIFY(not e->isTLogCRC() or
        //        checksum_.getValue() == e->getCheckSum());
        checksum_.update(e, Entry::getDataSize());
        return e;
    }
    else
    {
        return 0;
    }
}
}

// Local Variables: **
// End: **
