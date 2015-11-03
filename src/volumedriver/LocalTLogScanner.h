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
