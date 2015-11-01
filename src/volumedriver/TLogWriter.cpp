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

#include "BackwardTLogReader.h"
#include "TLogWriter.h"
#include "ClusterLocation.h"
#include "VolumeDriverError.h"

#include <youtils/FileUtils.h>
#include <youtils/FileDescriptor.h>

namespace volumedriver
{

template<bool synch,
         typename... Args>
void
TLogWriter::place(Args... args)
{
    try
    {
        maybe_refresh_buffer_(ForceWriteIfDirty::F);
        const Entry* e = new (current_entry_) Entry(std::forward<Args>(args)...);

        checksum_.update(current_entry_, sizeof(*e));
        ++current_entry_;
        ++entriesWritten_;
        if (synch)
        {
            maybe_refresh_buffer_(ForceWriteIfDirty::T);
            file_->sync();
        }
    }
    CATCH_STD_ALL_EWHAT({
            VolumeDriverError::report(events::VolumeDriverErrorCode::WriteTLog,
                                      EWHAT);
            throw;
        });
}

TLogWriter::TLogWriter(const fs::path& path,
                       const CheckSum* checksum,
                       ssize_t numBuffered)
    :
    // file_(path.string())
    // ,
    entriesWritten_(0)
    , lastClusterLocation()
    , buf(Entry::getDataSize() * numBuffered)
    , current_entry_(reinterpret_cast<Entry*>(&buf[0]))
    , last_entry_(current_entry_ + numBuffered)
{
    VERIFY(not checksum);

    try
    {
        CheckSum cs;

        if(fs::exists(path) and
           fs::file_size(path) != 0)
        {
            int64_t fl = fs::file_size(path);
            if(fl % Entry::getDataSize() != 0)
            {
                // file_ will be closed in its destructor
                std::stringstream ss;
                ss << "trailing garbage in tlog " << path << ": " <<
                    (fl % Entry::getDataSize()) << " bytes";
                LOG_ERROR(ss.str());
                throw fungi::IOException(ss.str().c_str());
            }
            cs =  FileUtils::calculate_checksum(path);
            VERIFY(checksum == 0 or
                   cs == *checksum);

            lastClusterLocation = BackwardTLogReader(path).nextClusterLocation();
            LOG_INFO("Starting an existing tlog " << path << " with clusterLocation " << lastClusterLocation);
        }

        checksum_ = checksum ? *checksum : cs;
        file_ = std::make_unique<TLogFile>(path.string(),
                                           FDMode::Write,
                                           CreateIfNecessary::T);

        // file_.open(FDMode::Write,
        //            CreateIfNecessary::T);
        file_->seek(0,
                    Whence::SeekEnd);

        LOG_DEBUG("writing to TLog " << path);
    }
    CATCH_STD_ALL_EWHAT({
            VolumeDriverError::report(events::VolumeDriverErrorCode::WriteTLog,
                                      EWHAT);
            throw;
        });
}

TLogWriter::~TLogWriter()
{
    maybe_refresh_buffer_(ForceWriteIfDirty::T);
}

void
TLogWriter::maybe_refresh_buffer_(ForceWriteIfDirty force)
{
    if((force == ForceWriteIfDirty::T and
        current_entry_ != reinterpret_cast<Entry*>(&buf[0])) or
       current_entry_ == last_entry_)
    {
        // VERIFY(file_.isOpen());
        file_->write(&buf[0],
                    reinterpret_cast<byte*>(current_entry_) - &buf[0]);
        current_entry_ = reinterpret_cast<Entry*>(&buf[0]);
    }
}

void
TLogWriter::add(const ClusterAddress address,
                const ClusterLocationAndHash& loc_and_hash)
{
    lastClusterLocation = loc_and_hash.clusterLocation;
    place<false>(address,
                 loc_and_hash);
}

void
TLogWriter::add(const CheckSum& cs)
{
    place<false>(cs,
                 Entry::Type::SCOCRC);
}

void
TLogWriter::add()
{
    place<true>();

}

CheckSum
TLogWriter::close()
{
    place<true>(checksum_,
                Entry::Type::TLogCRC);
    // file_.close();
    return checksum_;
}

void
TLogWriter::sync()
{
    maybe_refresh_buffer_(ForceWriteIfDirty::T);
    file_->sync();
}

void
TLogWriter::addWrongTLogCRC()
{
    place<true>(CheckSum(checksum_.getValue() + 1),
                Entry::Type::TLogCRC);
}

CheckSum
TLogWriter::getCheckSum() const
{
    return checksum_;
}

uint64_t
TLogWriter::getEntriesWritten() const
{
    return entriesWritten_;
}

}

// Local Variables: **
// End: **
