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
