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

#include "IncrementalChecksum.h"
#include "Assert.h"

namespace youtils
{

IncrementalChecksum::IncrementalChecksum(const fs::path& path,
                                         off_t offset)
    : f_(path, FDMode::Read)
    , current_offset_(0)
    , bufsize_(sysconf(_SC_PAGESIZE))
    , buf_(bufsize_)

{
    checksum(offset);
}

const CheckSum&
IncrementalChecksum::checksum(off_t offset)
{
    VERIFY(offset >= current_offset_);
    offset -= current_offset_;

    while(offset > 0)
    {
        const unsigned to_read = std::min(offset, bufsize_);
        ssize_t ret = f_.read(&buf_[0], to_read);
        if (ret != to_read)
        {
            throw CheckSumFileNotLargeEnough("checksum with offset, file not large enough");
        }

        checksum_.update(&buf_[0], to_read);
        offset -= to_read;
        current_offset_ +=to_read;

    }
    return checksum_;
}

}

// Local Variables: **
// End: **
