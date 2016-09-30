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

#include "ArakoonMetaDataBackend.h"
#include "BackendRestartAccumulator.h"
#include "CombinedTLogReader.h"
#include "CachedMetaDataStore.h"
#include "DataStoreNG.h"
#include "LocalTLogScanner.h"
#include "MDSMetaDataBackend.h"
#include "MDSMetaDataStore.h"
#include "MetaDataStoreBuilder.h"
#include "MetaDataStoreDebug.h"
#include "MetaDataStoreInterface.h"
#include "RocksDBMetaDataBackend.h"
#include "SCOCache.h"
#include "SnapshotManagement.h"
#include "TLogReader.h"
#include "TokyoCabinetMetaDataBackend.h"
#include "Types.h"
#include "VolManager.h"
#include "Volume.h"
#include "VolumeDriverError.h"
#include "VolumeFactory.h"

#include <cerrno>
#include <fstream>

#include <sys/types.h>
#include <dirent.h>

#include <boost/make_shared.hpp>

#include <youtils/Catchers.h>

#include <backend/Namespace.h>

namespace volumedriver
{

namespace be = backend;
namespace fs = boost::filesystem;
namespace yt = youtils;

namespace
{

DECLARE_LOGGER("VolumeFactory");

template<DeleteLocalData delete_local_meta_data,
         RemoveVolumeCompletely remove_volume_completely>
struct FreshVolumeDestroyer
{
    DECLARE_LOGGER("FreshVolumeDestroyer");

    void
    operator()(Volume* vol)
    {
        if(vol != nullptr)
        {
            try
            {
                vol->destroy(delete_local_meta_data,
                             remove_volume_completely,
                             ForceVolumeDeletion::T);
                delete vol;
            }
            CATCH_STD_ALL_LOG_IGNORE("Failed to destroy volume " <<
                                     vol->getName());
        }
    }

    void
    operator()(WriteOnlyVolume* vol)
    {
        if (vol != nullptr)
        {
            try
            {
                vol->destroy(remove_volume_completely);
                delete vol;
            }
            CATCH_STD_ALL_LOG_IGNORE("Failed to destroy write-only volume " <<
                                     vol->getName());
        }
    }
};

DataStoreNG*
make_data_store(const VolumeConfig& config)
{
    return new DataStoreNG(config,
                           VolManager::get()->getSCOCache(),
                           VolManager::get()->getOpenSCOsPerVolume());
}

std::unique_ptr<MetaDataStoreInterface>
make_metadata_store(const VolumeConfig& config,
                    const OwnerTag owner_tag)
{
    VolManager& vm = *VolManager::get();
    const size_t num_pages_cached = vm.effective_metadata_cache_capacity(config);

    MetaDataBackendInterfacePtr mdb;
    switch (config.metadata_backend_config_->backend_type())
    {
    case MetaDataBackendType::Arakoon:
        {
            const auto& mcfg(dynamic_cast<const ArakoonMetaDataBackendConfig&>(*config.metadata_backend_config_));
            mdb.reset(new ArakoonMetaDataBackend(mcfg,
                                                 config.getNS()));
            break;
        }
    case MetaDataBackendType::RocksDB:
        mdb.reset(new RocksDBMetaDataBackend(config));
        break;
    case MetaDataBackendType::TCBT:
        mdb.reset(new TokyoCabinetMetaDataBackend(config));
        break;
    case MetaDataBackendType::MDS:
        {
            const auto& mcfg(dynamic_cast<const MDSMetaDataBackendConfig&>(*config.metadata_backend_config_));
            const be::Namespace nspace(config.ns_);
            be::BackendInterfacePtr bi(vm.createBackendInterface(nspace));
            const fs::path home(vm.getMetaDataPath(nspace));
            return std::unique_ptr<MetaDataStoreInterface>(new MDSMetaDataStore(mcfg,
                                                                                std::move(bi),
                                                                                home,
                                                                                owner_tag,
                                                                                num_pages_cached));
        }
    }

    return std::unique_ptr<MetaDataStoreInterface>(new CachedMetaDataStore(mdb,
                                                                           config.ns_,
                                                                           num_pages_cached));
}

std::unique_ptr<MetaDataStoreInterface>
make_metadata_store(const VolumeConfig& config,
                    const ScrubId& sp_scrub_id,
                    const OwnerTag owner_tag)
{
    auto md(make_metadata_store(config,
                                owner_tag));
    const MaybeScrubId md_scrub_id(md->scrub_id());

    if (md_scrub_id != boost::none)
    {
        if (sp_scrub_id != *md_scrub_id)
        {
            LOG_ERROR(config.id_ <<
                      ": scrub ID mismatch - SnapshotPersistor thinks it should be " <<
                      sp_scrub_id << " while the MetaDataStore believes it is " <<
                      *md_scrub_id << " - clearing the MetaDataStore!");
            md->clear_all_keys();
        }
    }
    else
    {
        LOG_WARN(config.id_ <<
                 ": no scrub ID present in MetaDataStore - did we crash before writing it out?");
    }

    return md;
}

template<DeleteLocalData delete_local_data,
         RemoveVolumeCompletely remove_volume_completely>
std::unique_ptr<Volume, FreshVolumeDestroyer<delete_local_data,
                                             remove_volume_completely> >
create_volume_stage_1(const VolumeConfig& config,
                      const OwnerTag owner_tag,
                      const boost::shared_ptr<be::Condition>& backend_write_cond,
                      const RestartContext context,
                      NSIDMap nsid,
                      std::unique_ptr<MetaDataStoreInterface> md)
{
    std::unique_ptr<SnapshotManagement> sm(new SnapshotManagement(config,
                                                                  context));
    std::unique_ptr<DataStoreNG> ds(make_data_store(config));
    if (not md)
    {
        md = make_metadata_store(config,
                                 sm->scrub_id(),
                                 owner_tag);
    }

    const MaybeScrubId md_scrub_id(md->scrub_id());
    if (md_scrub_id != boost::none)
    {
    // Hey
#ifdef __clang_analyzer__
    //, trust me, the unique_ptr makes sure that this isn't leaked.
        if (md_scrub_id != sm->scrub_id())
        {
            ds = nullptr;
            ASSERT(false);
        }
#else
        VERIFY(md_scrub_id == sm->scrub_id());
#endif
    }
    else
    {
        md->set_scrub_id(sm->scrub_id());
    }

    // Hey
#ifdef __clang_analyzer__
    //, trust me, the unique_ptr makes sure that this isn't leaked.
    if (not nsid.get(0))
    {
        ds = nullptr;
        ASSERT(false);
    }
#else
    VERIFY(nsid.get(0));
#endif

    return std::unique_ptr<Volume, FreshVolumeDestroyer<delete_local_data,
                                                        remove_volume_completely> >
        (new Volume(config,
                    owner_tag,
                    backend_write_cond,
                    std::move(sm),
                    std::move(ds),
                    std::move(md),
                    std::move(nsid),
                    VolManager::get()->dtl_throttle_usecs.value(),
                    VolManager::get()->readOnlyMode()));
}

template<DeleteLocalData delete_local_data,
         RemoveVolumeCompletely remove_volume_completely>
std::unique_ptr<WriteOnlyVolume, FreshVolumeDestroyer<delete_local_data,
                                                      remove_volume_completely> >
create_write_only_volume_stage_1(const VolumeConfig& config,
                                 const OwnerTag owner_tag,
                                 const boost::shared_ptr<be::Condition>& backend_write_cond,
                                 const RestartContext context,
                                 NSIDMap nsid)
{
    std::unique_ptr<SnapshotManagement> sm(new SnapshotManagement(config,
                                                                  context));
    std::unique_ptr<DataStoreNG> ds(make_data_store(config));

    return std::unique_ptr<WriteOnlyVolume,
                           FreshVolumeDestroyer<delete_local_data,
                                                remove_volume_completely> >(new WriteOnlyVolume(config,
                                                                                                owner_tag,
                                                                                                backend_write_cond,
                                                                                                std::move(sm),
                                                                                                std::move(ds),
                                                                                                std::move(nsid)));
}

void
verify_absence_from_sco_cache(const VolumeConfig& config)
{
    SCOCache *scocache = VolManager::get()->getSCOCache();
    if (scocache->hasNamespace(config.getNS()))
    {
        LOG_WARN("Volume with namespace " << config.getNS() <<
                 " already exists in SCO cache");
        throw fungi::IOException("Volume (or part of it) already exists");
    }
}

void
verify_absence_from_metadata_path(const VolumeConfig& config)
{
    const fs::path mdpath(VolManager::get()->getMetaDataPath(config.getNS()));
    if (SnapshotManagement::exists(mdpath))
    {
        LOG_WARN("MetaDataStore already exists and is not empty under dir " <<
                 mdpath);
        throw fungi::IOException("Volume (or part of it) already exists");
    }
}

void
new_volume_prechecks(const VolumeConfig& config)
{
    verify_absence_from_metadata_path(config);
    verify_absence_from_sco_cache(config);
}

} // anon namespace

boost::shared_ptr<be::Condition>
VolumeFactory::claim_namespace(const be::Namespace& nspace,
                               const OwnerTag owner_tag)
{
    const std::string& name(VolumeInterface::owner_tag_backend_name());
    const fs::path p(yt::FileUtils::create_temp_file_in_temp_dir(nspace.str() + "." + name));
    ALWAYS_CLEANUP_FILE(p);

    fs::ofstream(p) << owner_tag;

    be::BackendInterfacePtr bi(VolManager::get()->createBackendInterface(nspace));

    try
    {
        return boost::make_shared<be::Condition>(name,
                                                 bi->write_tag(p,
                                                               name,
                                                               nullptr,
                                                               OverwriteObject::T));
    }
    catch (be::BackendNotImplementedException&)
    {
        return nullptr;
    }
}

std::unique_ptr<Volume>
VolumeFactory::createNewVolume(const VolumeConfig& config)
{
    new_volume_prechecks(config);

    NSIDMap nsid;
    nsid.set(0, VolManager::get()->createBackendInterface(config.getNS()));

    auto vol(create_volume_stage_1<DeleteLocalData::T,
                                   RemoveVolumeCompletely::T>(config,
                                                              config.owner_tag_,
                                                              claim_namespace(config.getNS(),
                                                                              config.owner_tag_),
                                                              RestartContext::Creation,
                                                              std::move(nsid),
                                                              nullptr));

    vol->newVolume();

    return std::unique_ptr<Volume>(vol.release());
}

std::unique_ptr<WriteOnlyVolume>
VolumeFactory::createWriteOnlyVolume(const VolumeConfig& config)
{
    new_volume_prechecks(config);

    NSIDMap nsid;
    nsid.set(0, VolManager::get()->createBackendInterface(config.getNS()));

    auto vol(create_write_only_volume_stage_1<DeleteLocalData::T,
                                              RemoveVolumeCompletely::T>(config,
                                                                         config.owner_tag_,
                                                                         claim_namespace(config.getNS(),
                                                                                         config.owner_tag_),
                                                                         RestartContext::Creation,
                                                                         std::move(nsid)));
    vol->newWriteOnlyVolume();

    return std::unique_ptr<WriteOnlyVolume>(vol.release());
}

namespace
{

boost::optional<yt::UUID>
get_implicit_start_cork(const VolumeConfig& config,
                        const SnapshotPersistor& sp)
{
    if(config.parent())
    {
        VERIFY(sp.parent());

        LOG_INFO("Volume was clone, checking for parent cork " << config.getNS());
        auto psp(SnapshotManagement::createSnapshotPersistor(VolManager::get()->createBackendInterface(config.parent()->nspace)));
        return psp->getSnapshotCork(sp.parent()->snapshot);
    }
    else
    {
        VERIFY(not sp.parent());
        return boost::none;
    }
}

void
check_clone_tlogs_in_backend(const VolumeConfig& config,
                             const NSIDMap& nsid,
                             const CloneTLogs& clone_tlogs)
{
    LOG_INFO(config.id_ << ": checking whether all required TLogs are in the backend");

    for (const auto& pair : clone_tlogs)
    {
        BackendInterfacePtr bi = nsid.get(pair.first)->clone();

        for (const auto& tlog_id : pair.second)
        {
            if (not bi->objectExists(boost::lexical_cast<std::string>(tlog_id)))
            {
                LOG_ERROR(config.id_ << ": TLog " << tlog_id <<
                          " does not exist in namespace " << bi->getNS());
                throw fungi::IOException("TLog missing from the backend");
            }
        }
    }
}

void
replay_tlogs_on_backend_since_last_cork(const VolumeConfig& config,
                                        MetaDataStoreInterface& mdstore,
                                        const SnapshotPersistor& sp)
try
{
    const boost::optional<yt::UUID> sp_cork(sp.lastCork());
    const boost::optional<yt::UUID> md_cork(mdstore.lastCork());

    if (config.parent() and not md_cork)
    {
        LOG_WARN("Volume was clone but has no MetaDataStore cork - we need to rebuild from scratch");
        mdstore.clear_all_keys();

        VolManager& vm = *VolManager::get();
        const fs::path scratch_path(vm.getMetaDataPath(config.getNS()) / "scratch");
        MetaDataStoreBuilder builder(mdstore,
                                     vm.createBackendInterface(config.getNS()),
                                     scratch_path);

        builder();
    }
    else
    {
        const boost::optional<yt::UUID> start_cork(get_implicit_start_cork(config,
                                                                           sp));

        LOG_INFO("Snapshot Cork: " << sp_cork << ", MetaDataStore Cork: " << md_cork <<
                 ", implicit start Cork " << start_cork);

        OrderedTLogIds tlogs_to_replay_now(sp.getTLogsOnBackendSinceLastCork(md_cork,
                                                                               start_cork));

        if (not tlogs_to_replay_now.empty())
        {
            VERIFY(sp_cork != boost::none);

            CloneTLogs clone_tlogs_to_replay;
            clone_tlogs_to_replay.emplace_back(SCOCloneID(0),
                                               tlogs_to_replay_now);
            NSIDMap nsid_map;

            {
                BackendInterfacePtr bi(VolManager::get()->createBackendInterface(config.getNS()));
                nsid_map.set(SCOCloneID(0),
                             std::move(bi));
            }

            check_clone_tlogs_in_backend(config,
                                         nsid_map,
                                         clone_tlogs_to_replay);

            // sync here so the cork reflects the last tlog on backend
            LOG_INFO("Replaying " << tlogs_to_replay_now.size() << " on the mdstore");

            LOG_INFO("First TLog to replay: " << tlogs_to_replay_now.front()
                     << ", last TLog to replay: " << tlogs_to_replay_now.back());

            mdstore.processCloneTLogs(clone_tlogs_to_replay,
                                      nsid_map,
                                      VolManager::get()->getTLogPath(config),
                                      true,
                                      sp_cork);
        }
    }
}
CATCH_STD_ALL_LOG_THROWFUNGI("Local restart not possible, exception during replay of TLogs from backend");

void
trim_current_tlogs(SnapshotPersistor& sp,
                   const VolumeConfig& config,
                   const TLogId& tlog_id)
{
    TLogReader treader(VolManager::get()->getTLogPath(config) /
                       boost::lexical_cast<std::string>(tlog_id));
    uint64_t newsize = 0;
    const Entry* e;

    while ((e = treader.nextLocation()))
    {
        newsize += config.getClusterSize();
    }

    const auto res = sp.snip(tlog_id,
                             newsize);
    VERIFY(res);

    sp.saveToFile(VolManager::get()->getSnapshotsPath(config),
                  SyncAndRename::T);
}

// This one walks over the remaining TLogs and checks their sanity /
// the sanity of the referenced SCOs. If all goes well the metadata is updated
// in memory but not just yet in the metadata backend as it's held back by corks.
// These are unplugged at a later stage of the local restart, once the SCOs
// and TLogs have hit the backend (using the "normal" mechanism in the
// callback after TLog writeout - cf. Volume::localRestart()).
void
replay_tlogs_not_on_backend(const VolumeConfig& config,
                            MetaDataStoreInterface& mdstore,
                            SnapshotPersistor& sp)
{
    const std::string vname(config.id_);
    LOG_INFO(vname <<
             ": checking local TLog and SCO checksums and preparing TLog replay");

    OrderedTLogIds tlogs_not_on_backend;
    sp.getTLogsNotWrittenToBackend(tlogs_not_on_backend);
    LocalTLogScanner tlog_scanner(config,
                                  mdstore);

    for (const auto& tlog_id : tlogs_not_on_backend)
    {
        LOG_INFO(vname << ": checking local TLog " << tlog_id <<
                 " for restart consistency and preparing replay");
        tlog_scanner.scanTLog(tlog_id);
    }

    TLogId tlog_id;

    if (tlog_scanner.isAborted())
    {
        LOG_WARN(vname <<
                 ": scan of local TLogs detected problems, reusing as much as possible");
        // LOG_WARN(vname << ": aborted TLog detected, refusing local restart");
        // throw AbortException("Aborted TLog detected");

        const LocalTLogScanner::TLogIdAndSize& tnas =
            tlog_scanner.last_good_tlog();
        tlog_id = tnas.first;
        FileUtils::truncate(VolManager::get()->getTLogPath(config) /
                            boost::lexical_cast<std::string>(tlog_id),
                            tnas.second * Entry::getDataSize());
    }
    else
    {
        const OrderedTLogIds current_ones(sp.getCurrentTLogs());
        VERIFY(not current_ones.empty());
        tlog_id = current_ones.back();
    }

    trim_current_tlogs(sp,
                       config,
                       tlog_id);
}

std::unique_ptr<MetaDataStoreInterface>
local_restart_metadata_store(const VolumeConfig& config,
                             const OwnerTag owner_tag)
{
    SnapshotPersistor sp(VolManager::get()->getSnapshotsPath(config));
    std::unique_ptr<MetaDataStoreInterface> mdstore(make_metadata_store(config,
                                                                        sp.scrub_id(),
                                                                        owner_tag));

    LOG_INFO("Trying to get the cork from the SnapshotPersistor");

    replay_tlogs_on_backend_since_last_cork(config,
                                            *mdstore,
                                            sp);

    replay_tlogs_not_on_backend(config,
                                *mdstore,
                                sp);

    // check_last_tlog(config,
    //                 sp);
    return mdstore;
}

class NSIDMapBuilder
{
public:
    explicit NSIDMapBuilder(NSIDMap& nsid)
        : nsid_map_(nsid)
    {}

    void
    operator()(const SnapshotPersistor&,
               BackendInterfacePtr& bi,
               const std::string& /* snapshot_name */,
               SCOCloneID clone_id)
    {
        nsid_map_.set(clone_id,
                      bi->clone());
    }

    static const FromOldest direction = FromOldest::T;

private:
    DECLARE_LOGGER("NSIDMapBuilder");
    NSIDMap& nsid_map_;
};

}

std::unique_ptr<Volume>
VolumeFactory::local_restart_volume(const VolumeConfig& config,
                                    const OwnerTag owner_tag,
                                    const IgnoreFOCIfUnreachable)
try
{
    boost::shared_ptr<be::Condition> backend_write_cond(claim_namespace(config.getNS(),
                                                                        owner_tag));

    std::unique_ptr<MetaDataStoreInterface>
        mdstore(local_restart_metadata_store(config,
                                             owner_tag));
    NSIDMap nsid;
    SnapshotPersistor sp(config.parent());
    NSIDMapBuilder builder(nsid);

    sp.vold(builder,
            VolManager::get()->createBackendInterface(config.getNS()));

    auto vol(create_volume_stage_1<DeleteLocalData::F,
                                   RemoveVolumeCompletely::F>(config,
                                                              owner_tag,
                                                              backend_write_cond,
                                                              RestartContext::LocalRestart,
                                                              std::move(nsid),
                                                              std::move(mdstore)));
    vol->localRestart();
    return std::unique_ptr<Volume>(vol.release());
}
CATCH_STD_ALL_LOGLEVEL_RETHROW("Cannot restart volume for namespace " <<
                               config.getNS() <<
                               " locally, reason should be in the logs",
                               WARN)

namespace
{

std::unique_ptr<Volume>
fall_back_to_backend_restart(const VolumeConfig& config,
                             const OwnerTag owner_tag,
                             const IgnoreFOCIfUnreachable ignore_foc)
{
    LOG_INFO(config.getNS() <<
             ": checking feasibility of falling back to a backend restart");

    auto vm = VolManager::get();

    FileUtils::checkDirectoryEmptyOrNonExistant(vm->getTLogPath(config));
    FileUtils::checkDirectoryEmptyOrNonExistant(vm->getMetaDataPath(config));

    be::BackendInterfacePtr bi(vm->createBackendInterface(config.getNS()));
    SnapshotPersistor sp(bi);
    const OrderedTLogIds dirty_tlogs(sp.getTLogsNotWrittenToBackend());
    if (dirty_tlogs.size() != 1)
    {
        LOG_ERROR(config.getNS() <<
                  ": not all TLogs are available on the backend - refusing to fall back to backend restart: " << dirty_tlogs);
        throw fungi::IOException("Not all TLogs on backend, refusing fall back to backend restart",
                                 config.getNS().str().c_str());
    }

    return VolumeFactory::backend_restart(config,
                                          owner_tag,
                                          PrefetchVolumeData::F,
                                          ignore_foc);
}

}

std::unique_ptr<Volume>
VolumeFactory::local_restart(const VolumeConfig& config,
                             const OwnerTag owner_tag,
                             const FallBackToBackendRestart fallback,
                             const IgnoreFOCIfUnreachable ignore_foc)
{
    if (fs::exists(VolManager::get()->getSnapshotsPath(config)))
    {
        return local_restart_volume(config,
                                    owner_tag,
                                    ignore_foc);
    }
    else if (fallback == FallBackToBackendRestart::T)
    {
        return fall_back_to_backend_restart(config,
                                            owner_tag,
                                            ignore_foc);
    }
    else
    {
        throw fungi::IOException("Failed to restart volume: no local snapshots data and no fallback to backend permitted",
                                 config.getNS().str().c_str());
    }
}

std::unique_ptr<Volume>
VolumeFactory::backend_restart(const VolumeConfig& config,
                               const OwnerTag owner_tag,
                               const PrefetchVolumeData prefetch,
                               const IgnoreFOCIfUnreachable ignoreFOCIfUnreachable)
{
    const be::Namespace nspace(config.getNS());

    LOG_INFO("Restarting volume with namespace " << nspace << " from backend");

    auto vm = VolManager::get();
    FileUtils::checkDirectoryEmptyOrNonExistant(vm->getTLogPath(config));
    FileUtils::checkDirectoryEmptyOrNonExistant(vm->getMetaDataPath(config));

    // If we're beyond the above checks any data in the SCOCache namespace
    // can go as it's either safely on the backend or not referenced by local
    // tlogs anymore.
    vm->getSCOCache()->removeDisabledNamespace(nspace);

    const boost::shared_ptr<be::Condition> backend_write_cond(claim_namespace(config.getNS(),
                                                                              owner_tag));

    try
    {
        BackendInterfacePtr bi(vm->createBackendInterface(nspace));
        LOG_TRACE("reading the snapshots file " <<
                  snapshotFilename() <<
                  " into " <<
                  vm->getSnapshotsPath(config));

        try
        {
            bi->read(vm->getSnapshotsPath(config),
                     snapshotFilename(),
                     InsistOnLatestVersion::T);
        }
        CATCH_STD_ALL_EWHAT({
                VolumeDriverError::report(events::VolumeDriverErrorCode::GetSnapshotsFromBackend,
                                          EWHAT,
                                          config.id_);
                throw;
            });

        LOG_INFO("Getting the TLOGS for this volume");
        SnapshotPersistor sp(vm->getSnapshotsPath(config));

        sp.trimToBackend();
        sp.saveToFile(VolManager::get()->getSnapshotsPath(config),
                      SyncAndRename::T);

        std::unique_ptr<MetaDataStoreInterface>
            mdstore(make_metadata_store(config,
                                        sp.scrub_id(),
                                        owner_tag));

        const boost::optional<yt::UUID> maybe_cork(mdstore->lastCork());

        NSIDMap nsid;
        BackendRestartAccumulator acc(nsid,
                                      maybe_cork,
                                      boost::none);
        sp.vold(acc,
                bi->clone());

        const CloneTLogs& restartTLogs(acc.clone_tlogs());

        VERIFY(restartTLogs.size() <= nsid.size());

        LOG_INFO("Trying to find out restart sconumber");

        VERIFY(restartTLogs.back().first == SCOCloneID(0));

        // To figure out the restart SCO, we need to look at all TLogs (i.e.
        // restartTLogs.back().second is *not* sufficient as that might point to an empty
        // TLog in case the cork in the mdstore is pretty much up to date).
        const OrderedTLogIds latest_tlogs(sp.getTLogsWrittenToBackend());
        SCONumber restart_sco_num = 0;

        FileUtils::with_temp_dir(vm->getTLogPath(config) / "tmp",
                                 [&](const fs::path& tmp)
                                 {
                                     auto t(CombinedTLogReader::create_backward_reader(tmp,
                                                                                       latest_tlogs,
                                                                                       bi->clone()));
                                     restart_sco_num = t->nextClusterLocation().number();

                                 });

        if (restart_sco_num == 0)
        {
            LOG_WARN("Could not find a SCO, restarting datastore from 0");
        }

        auto vol(create_volume_stage_1<DeleteLocalData::T,
                                       RemoveVolumeCompletely::F>(config,
                                                                  owner_tag,
                                                                  backend_write_cond,
                                                                  RestartContext::BackendRestart,
                                                                  std::move(nsid),
                                                                  std::move(mdstore)));
        vol->backend_restart(restartTLogs,
                             restart_sco_num,
                             ignoreFOCIfUnreachable,
                             sp.lastCork());
        if (T(prefetch))
        {
            try
            {
                vol->startPrefetch(restart_sco_num);
            }
            catch (std::exception& e)
            {
                LOG_WARN(vol->getName() <<
                         ": starting prefetching failed: " << e.what());
                // swallow exception
            }
        }
        return std::unique_ptr<Volume>(vol.release());
    }
    catch (...)
    {
        fs::remove_all(vm->getMetaDataPath(config));
        fs::remove_all(vm->getTLogPath(config));

        throw;
    }
}

std::unique_ptr<WriteOnlyVolume>
VolumeFactory::backend_restart_write_only_volume(const VolumeConfig& config,
                                                 const OwnerTag owner_tag)
{
    const be::Namespace nspace(config.getNS());

    LOG_INFO("Restarting Write Only volume with namespace " << nspace <<
             " from backend, owner tag " << owner_tag);

    auto vm = VolManager::get();
    FileUtils::checkDirectoryEmptyOrNonExistant(vm->getMetaDataPath(config));
    FileUtils::checkDirectoryEmptyOrNonExistant(vm->getTLogPath(config));

    // if we're beyond the above checks any data in the SCOCache namespace
    // can go as it's either safely on the backend or not referenced by local
    // tlogs anymore.
    vm->getSCOCache()->removeDisabledNamespace(nspace);

    const boost::shared_ptr<be::Condition> backend_write_cond(claim_namespace(config.getNS(),
                                                                              owner_tag));

    NSIDMap nsid;
    nsid.set(0,
             VolManager::get()->createBackendInterface(nspace));

    BackendInterfacePtr bi = nsid.get(0)->clone();

    LOG_TRACE("reading the snapshots file " <<
              snapshotFilename() <<
              " into " <<
              VolManager::get()->getSnapshotsPath(config));

    try
    {
        bi->read(VolManager::get()->getSnapshotsPath(config),
                 snapshotFilename(),
                 InsistOnLatestVersion::T);
    }
    CATCH_STD_ALL_EWHAT({
            VolumeDriverError::report(events::VolumeDriverErrorCode::GetSnapshotsFromBackend,
                                      EWHAT,
                                      config.id_);
            throw;
        });

    SnapshotPersistor sp(VolManager::get()->getSnapshotsPath(config));

    sp.trimToBackend();

    sp.saveToFile(VolManager::get()->getSnapshotsPath(config),
                  SyncAndRename::T);

    OrderedTLogIds recentTLogs(sp.getTLogsWrittenToBackend());

    SCONumber restart_sco_num = 0;
    FileUtils::with_temp_dir(vm->getTLogPath(config) / "tmp",
                             [&](const fs::path& tmp)
                             {
                                 auto t(CombinedTLogReader::create_backward_reader(tmp,
                                                                                   recentTLogs,
                                                                                   bi->clone()));
                                 restart_sco_num = t->nextClusterLocation().number();
                             });

    if (restart_sco_num == 0)
    {
        LOG_WARN("Could not find a SCO, restarting datastore from 0");
    }

    auto v(create_write_only_volume_stage_1<DeleteLocalData::T,
                                            RemoveVolumeCompletely::F>(config,
                                                                       owner_tag,
                                                                       backend_write_cond,
                                                                       RestartContext::BackendRestart,
                                                                       std::move(nsid)));
    v->backend_restart(restart_sco_num);
    return std::unique_ptr<WriteOnlyVolume>(v.release());
}

namespace
{

struct CloneFromParentSnapshotAcc
{
    DECLARE_LOGGER("CloneFromParentSnapshotAcc")

    CloneFromParentSnapshotAcc(CloneTLogs& clone_tlogs,
                               NSIDMap& nsid_map)
        : clone_tlogs_(clone_tlogs)
        , nsid_map_(nsid_map)
    {
        VERIFY(nsid_map.empty());
    };

    void
    operator()(const SnapshotPersistor& sp,
               BackendInterfacePtr& bi,
               const SnapshotName& snap_name,
               const SCOCloneID clone_id)
    {
        if(clone_id == SCOCloneID(0))
        {
            clone_tlogs_.push_back(std::make_pair(clone_id, OrderedTLogIds()));
            nsid_map_.set(clone_id,
                          bi->clone());
        }
        else
        {
            VERIFY(not snap_name.empty());
            OrderedTLogIds tlogs;
            sp.getTLogsTillSnapshot(snap_name, tlogs);
            clone_tlogs_.push_back(std::make_pair(clone_id, std::move(tlogs)));
            nsid_map_.set(clone_id,
                          bi->clone());
        }
    }

    static const FromOldest direction = FromOldest::T;

    CloneTLogs& clone_tlogs_;
    NSIDMap& nsid_map_;
};

}

std::unique_ptr<Volume>
VolumeFactory::createClone(const VolumeConfig& config,
                           const PrefetchVolumeData prefetch,
                           const yt::UUID& parent_snap_uuid)
{
    NSIDMap nsid;
    CloneTLogs ctbrf;

    CloneFromParentSnapshotAcc acc(ctbrf, nsid);
    SnapshotPersistor s(config.parent());

    s.vold(acc,
           VolManager::get()->createBackendInterface(config.getNS()));

    VERIFY(ctbrf.size() == nsid.size());

    auto vol(create_volume_stage_1<DeleteLocalData::T,
                                   RemoveVolumeCompletely::T>(config,
                                                              config.owner_tag_,
                                                              claim_namespace(config.getNS(),
                                                                              config.owner_tag_),
                                                              RestartContext::Cloning,
                                                              std::move(nsid),
                                                              nullptr));

    vol->cloneFromParentSnapshot(parent_snap_uuid,
                                 ctbrf);

    if (T(prefetch))
    {
        try
        {
            vol->startPrefetch(0);
        }
        catch (std::exception& e)
        {
            LOG_WARN("" << vol->getName() << ": starting prefetching failed: " <<
                     e.what());
            // swallow exception
        }
    }

    return std::unique_ptr<Volume>(vol.release());
}

uint64_t
VolumeFactory::metadata_locally_used_bytes(const VolumeConfig& cfg)
{
    switch (cfg.metadata_backend_config_->backend_type())
    {
    case MetaDataBackendType::Arakoon:
        return ArakoonMetaDataBackend::locally_used_bytes(cfg);
    case MetaDataBackendType::MDS:
        return MDSMetaDataBackend::locally_used_bytes(cfg);
    case MetaDataBackendType::RocksDB:
        return RocksDBMetaDataBackend::locally_used_bytes(cfg);
    case MetaDataBackendType::TCBT:
        return TokyoCabinetMetaDataBackend::locally_used_bytes(cfg);
    }

    UNREACHABLE;
}

uint64_t
VolumeFactory::metadata_locally_required_bytes(const VolumeConfig& cfg)
{
    switch (cfg.metadata_backend_config_->backend_type())
    {
    case MetaDataBackendType::Arakoon:
        return ArakoonMetaDataBackend::locally_required_bytes(cfg);
    case MetaDataBackendType::MDS:
        return MDSMetaDataBackend::locally_required_bytes(cfg);
    case MetaDataBackendType::RocksDB:
        return RocksDBMetaDataBackend::locally_required_bytes(cfg);
    case MetaDataBackendType::TCBT:
        return TokyoCabinetMetaDataBackend::locally_required_bytes(cfg);
    }

    UNREACHABLE;
}

}

// Local Variables: **
// mode: c++ **
// End: **
