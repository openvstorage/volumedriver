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
VD_BOOLEAN_ENUM(ForceWriteIfDirty)
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
