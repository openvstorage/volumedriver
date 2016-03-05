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
