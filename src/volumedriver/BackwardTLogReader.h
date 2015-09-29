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

#ifndef BACKWARD_TLOG_READER
#define BACKWARD_TLOG_READER

#include "Entry.h"
#include "OneFileTLogReader.h"

#include <time.h>

#include <iosfwd>
#include <vector>

#include <youtils/Assert.h>
#include <youtils/Logging.h>
#include <youtils/FileDescriptor.h>

#include <backend/BackendInterface.h>

namespace volumedriver
{

class BackwardTLogReader
    : public OneFileTLogReader
{
public:
    BackwardTLogReader(const fs::path& TLogPath,
                       const std::string& tlogName,
                       BackendInterfacePtr bi,
                       uint32_t cache_size = max_num_cached);

    explicit BackwardTLogReader(const fs::path& path,
                                uint32_t cache_size = max_num_cached);

    virtual ~BackwardTLogReader();

    const Entry*
    nextAny();

private:
    DECLARE_LOGGER("BackwardTLogReader");

    const static uint32_t max_num_cached = 256*256;
    ssize_t buf_pos;
    std::vector<Entry> entries;
    const uint64_t buf_size ;

    bool
    maybe_refresh_buffer();
};

}

#endif // BACKWARD_TLOG_READER

// Local Variables: **
// mode: c++ **
// End: **
