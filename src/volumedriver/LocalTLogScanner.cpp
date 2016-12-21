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

#include "LocalTLogScanner.h"
#include "MetaDataStoreInterface.h"
#include "SnapshotPersistor.h"
#include "TLogReaderInterface.h"
#include "VolManager.h"
#include "VolumeConfig.h"

#include <youtils/UUID.h>

namespace volumedriver
{

namespace yt = youtils;

LocalTLogScanner::LocalTLogScanner(const VolumeConfig& volume_config,
                                   MetaDataStoreInterface& mdstore)
        : volume_config_(volume_config)
        , zcovetcher_(volume_config)
        , mdstore_(mdstore)
        , aborted_(false)
        , last_good_tlog_(TLogId(yt::UUID::NullUUID()), 0)
        , tlogs_path_(VolManager::get()->getTLogPath(volume_config_))
        , tlog_without_final_crc_(false)
{}

void
LocalTLogScanner::processLoc(ClusterAddress ca,
                             const ClusterLocationAndHash& loc_and_hash)
{
    ASSERT(not aborted_);

    LOG_TRACE("Processing clusteraddress " << ca << ", loc " << loc_and_hash);
    replay_queue_.emplace_back(ca, loc_and_hash);
}

void
LocalTLogScanner::processSCOCRC(CheckSum::value_type t)
{
    ASSERT(not aborted_);
    ASSERT(current_proc_);

    const ClusterLocation& loc(current_proc_->current_clusterlocation_);

    LOG_INFO("Processing SCO checksum " << t << " for " << loc);

    if(zcovetcher_.checkSCO(loc,
                            t,
                            true))
    {
        LOG_INFO("Verified SCO checksum for " << loc.sco() << " - replaying it");

        for (const auto& p : replay_queue_)
        {
            mdstore_.writeCluster(p.first, p.second);
        }

        replay_queue_.clear();

        //num_entries + 1 as current_proc is only update after this call
        last_good_tlog_.second = current_proc_->num_entries_ + 1;
    }
    else
    {
        LOG_WARN("SCO checksum verification failed for SCO " << loc.sco());
        throw AbortException("Failed to verify SCO checksum");
    }
}

void
LocalTLogScanner::processTLogCRC(CheckSum::value_type /*t*/)
{
    ASSERT(not aborted_);
}

void
LocalTLogScanner::processSync()
{
    ASSERT(not aborted_);
}

void
LocalTLogScanner::scanTLog(const TLogId& tlog_id)
{
    if(tlog_without_final_crc_)
    {
        ASSERT(aborted_);
        ASSERT(current_proc_);
        LOG_ERROR("Seen a TLog " << current_proc_->tlog_id() <<
                  " without a final TLog CRC, but other TLogs after it");
        throw TLogWithoutFinalCRC("TLog has no CRC and TLogs following");
    }

    const auto tlog_name(boost::lexical_cast<std::string>(tlog_id));
    const fs::path tlog_path(tlogs_path_ / tlog_name);

    if (aborted_)
    {
        LOG_INFO("Removing TLog " << tlog_id <<
                 " because a problem was found in an earlier TLog");
        fs::remove(tlog_path);
        return;
    }

    VERIFY(replay_queue_.empty());

    LOG_INFO("Checking TLog " << tlog_id);

    last_good_tlog_.first = tlog_id;
    last_good_tlog_.second = 0;

    mdstore_.cork(tlog_id);

    current_proc_.reset(new CheckTLogAndSCOCRCProcessor(tlog_id));
    auto proc = make_combined_processor(*this, *current_proc_);
    TLogReader tlog_reader(tlog_path);

    bool current_tlog_scanned_to_the_end = true;
    try
    {
        tlog_reader.for_each(proc);
    }
    catch(AbortException&)
    {
        current_tlog_scanned_to_the_end = false;
        aborted_ = true;
    }

    if(current_tlog_scanned_to_the_end and (not current_proc_->seen_final_sco_crc_))
    {
        LOG_WARN("Not seen a final SCO CRC in the TLogs");
        aborted_ = true;
    }
    if(current_tlog_scanned_to_the_end and (not current_proc_->last_entry_was_tlog_crc_))
    {
        LOG_WARN("No final TLog CRC seen");
        aborted_ = true;
        tlog_without_final_crc_ = true;
    }

    if(aborted_)
    {
        LOG_INFO("Cutting " << tlog_name << " at clusteroffset " <<
                 (last_good_tlog_.second + 1));
        //Assert relies on the fact that
        //  - last_good_tlog_size_ is only updated when seen a SCOCRC
        //  - a SCOCRC is always preceded by at least one LOC entry
        ASSERT(last_good_tlog_.second != 1);
        replay_queue_.clear();
    }

    VERIFY(replay_queue_.empty());
}

const LocalTLogScanner::TLogIdAndSize&
LocalTLogScanner::last_good_tlog() const
{
    VERIFY(last_good_tlog_.first != TLogId(yt::UUID::NullUUID()));
    return last_good_tlog_;
}

}
