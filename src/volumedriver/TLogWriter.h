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

#ifndef TLOGWRITER_H_
#define TLOGWRITER_H_

#include <boost/utility.hpp>

#include "Entry.h"
#include <time.h>
#include <vector>
#include <cassert>
#include <iosfwd>
#include "ClusterLocation.h"
#include <youtils/CheckSum.h>
#include <youtils/FileDescriptor.h>

namespace volumedriver
{

/**
 *  Transaction Log acccess, write (append) only role.
 */
BOOLEAN_ENUM(ForceWriteIfDirty)
class TLogWriter
{
public:

    explicit TLogWriter(const fs::path& file,
                        const CheckSum* = 0,
                        ssize_t numBuffered = 64);

    ~TLogWriter();

    TLogWriter(const TLogWriter&) = delete;

    TLogWriter&
    operator=(const TLogWriter&) = delete;

    CheckSum
    close();

    void
    sync();

    // Address-Cluster Entry
    void
    add(const ClusterAddress address,
        const ClusterLocationAndHash& location);

    // CRC Entry For a SCO
    void
    add(const CheckSum& cs);

    // SyncTC Entry
    void
    add();

    const ClusterLocation&
    getClusterLocation() const
    {
        return lastClusterLocation;
    }

    CheckSum
    getCheckSum() const;

    // Wanna get rid of this too.
    uint64_t
    getEntriesWritten() const;

    // Testing my dear, testing
    void
    addWrongTLogCRC();

private:
    DECLARE_LOGGER("TLogWriter");

    typedef ::youtils::FileDescriptor TLogFile;
    std::unique_ptr<youtils::FileDescriptor> file_;

    CheckSum checksum_;
    uint64_t entriesWritten_;

    ClusterLocation lastClusterLocation;
    std::vector<byte> buf;

    Entry* current_entry_;
    const Entry* last_entry_;

    void
    maybe_refresh_buffer_(ForceWriteIfDirty);

    template<bool sync,
             typename... Args>
    void place(Args... args);

};

}

// Local Variables: **
// End: **

#endif /* TLOGWRITER_H_ */
