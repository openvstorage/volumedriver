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

    virtual ~TLogReader() = default;

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
