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

#ifndef TLOGREADER_H_
#define TLOGREADER_H_


#include <time.h>
#include <vector>
#include <cassert>
#include <iosfwd>

#include <youtils/FileDescriptor.h>
#include "Entry.h"
#include "backend/BackendInterface.h"
#include <youtils/Logging.h>
#include "TLogReaderInterface.h"
#include "OneFileTLogReader.h"

namespace volumedriver
{

class TLogReader
    : public OneFileTLogReader
{

public:
    TLogReader(const fs::path& TLogPath,
               const std::string& TLogName,
               BackendInterfacePtr bi,
               uint32_t cache_size = max_num_cached);

    explicit TLogReader(const fs::path& path,
                        uint32_t cache_size = max_num_cached);


    virtual ~TLogReader();

    const Entry*
    nextAny();

    DECLARE_LOGGER("TLogReader");

private:
    const static uint32_t max_num_cached = 256*256;
    ssize_t buf_pos;
    std::vector<Entry> entries;
    ssize_t max_pos;
    const uint32_t buf_size;
    bool
    maybe_refresh_buffer();
    CheckSum checksum_;

};



}

#endif /* TLOGREADER_H_ */
// Local Variables: **
// End: **
