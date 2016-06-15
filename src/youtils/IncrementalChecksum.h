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

#ifndef INCREMENTAL_CHECKSUM_
#define INCREMENTAL_CHECKSUM_
#include "CheckSum.h"
#include "FileUtils.h"
#include "FileDescriptor.h"
namespace youtils
{
class IncrementalChecksum
{
public:
    IncrementalChecksum(const fs::path&,
                        off_t offset = 0);

    const CheckSum&
    checksum(off_t offset);
    DECLARE_LOGGER("IncrementalChecksum");

private:
    CheckSum checksum_;
    FileDescriptor f_;
    off_t current_offset_;
    const off_t bufsize_;
    std::vector<uint64_t> buf_;


};
}


#endif // INCREMENTAL_CHECKSUM_

// Local Variables: **
// mode: c++ **
// End: **
