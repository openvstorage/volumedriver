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
