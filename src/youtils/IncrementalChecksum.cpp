// This file is dual licensed GPLv2 and Apache 2.0.
// Active license depends on how it is used.
//
// Copyright 2016 iNuron NV
//
// // GPL //
// This file is part of OpenvStorage.
//
// OpenvStorage is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenvStorage. If not, see <http://www.gnu.org/licenses/>.
//
// // Apache 2.0 //
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
