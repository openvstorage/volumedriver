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

#ifndef LOCAL_TLOG_SCANNER_H_
#define LOCAL_TLOG_SCANNER_H_

#include "ClusterLocationAndHash.h"
#include "EntryProcessor.h"
#include "ZCOVetcher.h"

#include <memory>
#include <vector>

#include <youtils/IOException.h>
namespace volumedriver
{

class MetaDataStoreInterface;
class SnapshotPersistor;
class VolumeConfig;

MAKE_EXCEPTION(TLogRestartException, fungi::IOException);
MAKE_EXCEPTION(TLogWithoutFinalCRC, TLogRestartException);
MAKE_EXCEPTION(AbortException, TLogRestartException);

class LocalTLogScanner
    : public BasicDispatchProcessor
{
public:
    LocalTLogScanner(const VolumeConfig& volume_config,
                     MetaDataStoreInterface& mdstore);

    void
    scanTLog(const TLogId&);

    bool
    isAborted() const
    {
        return aborted_;
    }

    virtual void
    processLoc(ClusterAddress,
               const ClusterLocationAndHash&) override final;

    virtual void
    processSCOCRC(CheckSum::value_type) override final;

    virtual void
    processTLogCRC(CheckSum::value_type) override final;

    virtual void
    processSync() override final;

    using TLogIdAndSize = std::pair<TLogId, uint64_t>;

    const TLogIdAndSize&
    last_good_tlog() const;

private:
    DECLARE_LOGGER("LocalTLogScanner");

    std::unique_ptr<CheckTLogAndSCOCRCProcessor> current_proc_;
    const VolumeConfig& volume_config_;
    ZCOVetcher zcovetcher_;
    MetaDataStoreInterface& mdstore_;
    bool aborted_;
    TLogIdAndSize last_good_tlog_;
    const fs::path tlogs_path_;
    bool tlog_without_final_crc_;
    std::vector<std::pair<ClusterAddress,
                          ClusterLocationAndHash> > replay_queue_;
};

}

#endif // LOCAL_TLOG_SCANNER_H

// Local Variables: **
// mode: c++ **
// End: **
