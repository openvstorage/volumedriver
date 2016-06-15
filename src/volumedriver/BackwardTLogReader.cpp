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

#include "BackwardTLogReader.h"
#include <youtils/Assert.h>
#include "VolumeDriverError.h"
#include "youtils/FileUtils.h"

namespace volumedriver
{

BackwardTLogReader::BackwardTLogReader(const fs::path& TLogPath,
                                       const std::string& TLogName,
                                       BackendInterfacePtr bi,
                                       uint32_t cache_size)
    : OneFileTLogReader(TLogPath,
                        TLogName,
                        std::move(bi))
    , buf_pos(0)
    , entries(cache_size)
    , buf_size(cache_size * Entry::getDataSize())
{
    file_->seek(0, Whence::SeekEnd);
}

BackwardTLogReader::BackwardTLogReader(const fs::path& path,
                                       uint32_t cache_size)
    : OneFileTLogReader(path)
    , buf_pos(0)
    , entries(cache_size)
    , buf_size(cache_size * Entry::getDataSize())
{
    file_->seek(0, Whence::SeekEnd);
}

BackwardTLogReader::~BackwardTLogReader()
{}

bool
BackwardTLogReader::maybe_refresh_buffer()
{
    if(buf_pos == 0)
    {
        uint64_t pos = file_->tell();
        uint64_t min = std::min(buf_size, pos);
        off_t offset = file_->seek(- min, Whence::SeekCur);
        buf_pos = file_->pread(&entries[0], min, offset);
        VERIFY(buf_pos >= 0);
        VERIFY(buf_pos % Entry::getDataSize() == 0);
        VERIFY((uint64_t)buf_pos == min);
        buf_pos /= Entry::getDataSize();
        return (buf_pos != 0);
    }
    else
    {
        return true;
    }
}

const Entry*
BackwardTLogReader::nextAny()
{
    if(maybe_refresh_buffer())
    {
        Entry* e = &entries[0] + (--buf_pos);
        return e;
    }
    else
    {
        return 0;
    }
}
}

// Local Variables: **
// mode: c++ **
// End: **
