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

#include "BackendRestartAccumulator.h"
#include "MetaDataStoreBuilder.h"
#include "SnapshotPersistor.h"
#include "VolumeConfig.h"
#include "VolumeConfigPersistor.h"

#include <youtils/Catchers.h>

namespace volumedriver
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace yt = youtils;

MetaDataStoreBuilder::MetaDataStoreBuilder(MetaDataStoreInterface& mdstore,
                                           be::BackendInterfacePtr bi,
                                           const fs::path& scratch_dir)
    : mdstore_(mdstore)
    , bi_(std::move(bi))
    , scratch_dir_(scratch_dir)
{
    fs::create_directories(scratch_dir);
}

MetaDataStoreBuilder::~MetaDataStoreBuilder()
{
    try
    {
        fs::remove_all(scratch_dir_);
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to remove scratch directory " << scratch_dir_);
}

MetaDataStoreBuilder::Result
MetaDataStoreBuilder::operator()(const boost::optional<yt::UUID>& end_cork,
                                 CheckScrubId check_scrub_id,
                                 DryRun dry_run)
{
    try
    {
        const boost::optional<yt::UUID> start_cork(mdstore_.lastCork());
        return update_metadata_store_(start_cork,
                                      end_cork,
                                      check_scrub_id,
                                      dry_run,
                                      false);
    }
    catch (CorkNotFoundException& e)
    {
        LOG_INFO("cork not found on backend - could be caused by a snapshot rollback");
    }

    LOG_INFO(bi_->getNS() << ": retrying from clean slate");

    mdstore_.clear_all_keys();
    return update_metadata_store_(boost::none,
                                  end_cork,
                                  check_scrub_id,
                                  dry_run,
                                  true);
}

MetaDataStoreBuilder::Result
MetaDataStoreBuilder::update_metadata_store_(const boost::optional<yt::UUID>& from,
                                             const boost::optional<yt::UUID>& to,
                                             CheckScrubId check_scrub_id,
                                             DryRun dry_run,
                                             bool full_rebuild)
{
    LOG_INFO(bi_->getNS() <<
             ": bringing MetaDataStore in sync with backend, requested interval (" <<
             from << ", " << to << "], check scrub ID: " << check_scrub_id <<
             ", dry run:" << dry_run << ", full rebuild: " << full_rebuild);

    // This has a lot in common with the mdstore rebuilding code in
    // volumedriver::VolumeFactory - see if this can be unified.
    // In that case special care needs to be taken WRT file locations
    // (VolumeFactory::backend_restart wants to write snapshots.xml to
    // the correct path, ...).
    SnapshotPersistor sp(bi_);
    sp.trimToBackend();

    const ScrubId sp_scrub_id(sp.scrub_id());
    const MaybeScrubId md_scrub_id(mdstore_.scrub_id());

    Result res(sp_scrub_id);
    res.full_rebuild = full_rebuild;

    boost::optional<yt::UUID> start_cork(from);

    // Perhaps slightly more complex than necessary -
    // 'if (not md_scrub_id or *md_scrub_id != sp_scrub_id)' would do - but this
    // way the first build is accounted as an incremental build and not as a
    // full rebuild.
    if (check_scrub_id == CheckScrubId::T)
    {
        bool need_rebuild = false;

        if (md_scrub_id != boost::none)
        {
            if (*md_scrub_id != sp_scrub_id)
            {
                LOG_WARN(bi_->getNS() <<
                         ": scrub ID mismatch - SnapshotPersistor thinks it should be " <<
                         sp_scrub_id << " while the MetaDataStore believes it is " <<
                         *md_scrub_id);

                need_rebuild = true;

            }
        }
        else
        {
            LOG_INFO(bi_->getNS() << ": no scrub ID found in local metadata store");
            if (mdstore_.lastCork())
            {
                LOG_WARN(bi_->getNS() <<
                         ": old MDS slave detected: cork present but no scrub ID. " <<
                         "Rebuild required.");
                need_rebuild = true;
            }
        }

        if (need_rebuild)
        {
            if (dry_run == DryRun::T)
            {
                LOG_WARN(bi_->getNS() <<
                         ": assuming that the MetaDataStore has to be rebuilt from scratch");
            }
            else
            {
                LOG_WARN(bi_->getNS() << ": clearing the MetaDataStore!");
                mdstore_.clear_all_keys();
            }

            res.full_rebuild = true;
            start_cork = boost::none;
        }
    }

    const boost::optional<yt::UUID> end_cork(to == boost::none ?
                                             sp.lastCork() :
                                             *to);

    LOG_INFO(bi_->getNS() << ": adjusted interval (" << start_cork << ", " <<
             end_cork << "]");

    if (start_cork != boost::none and
        *start_cork == end_cork)
    {
        LOG_INFO(bi_->getNS() << ": requested interval already present, nothing to do");
    }
    else
    {
        // 'res.full_rebuild' indicates whether the old state was thrown away,
        // which does not cover the case of the mdstore being empty to begin with.
        const bool from_scratch = mdstore_.lastCork() == boost::none;
        ASSERT(not res.full_rebuild or from_scratch);

        VolumeConfig cfg;
        VolumeConfigPersistor::load(*bi_,
                                    cfg);

        LOG_INFO(bi_->getNS() << ": determining the TLogs to replay");

        NSIDMap nsid_map;
        BackendRestartAccumulator acc(nsid_map,
                                      start_cork,
                                      end_cork);

        sp.vold(acc,
                bi_->clone());

        const CloneTLogs& tlogs(acc.clone_tlogs());

        VERIFY(tlogs.size() <= nsid_map.size());
        VERIFY(not tlogs.empty());
        VERIFY(tlogs.back().first == SCOCloneID(0));

        for (const auto& p : tlogs)
        {
            res.num_tlogs += p.second.size();
        }

        if (dry_run == DryRun::F)
        {
            LOG_INFO(bi_->getNS() << ": replaying " << res.num_tlogs << " TLogs");

            mdstore_.processCloneTLogs(tlogs,
                                       nsid_map,
                                       scratch_dir_,
                                       true,
                                       end_cork);

            if (from_scratch or check_scrub_id == CheckScrubId::T)
            {
                mdstore_.set_scrub_id(sp_scrub_id);
            }
        }
    }

    return res;
}

}
