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

// This one needs to go first so it's defined in case any header drags
// in <youtils/Catchers> before we do it explicitly. Ugly? Hell yeah!
#define GETVOLUME() (this)

#include "BackendRestartAccumulator.h"
#include "BackendTasks.h"
#include "CombinedTLogReader.h"
#include "DataStoreNG.h"
#include "FailOverCacheClientInterface.h"
#include "MDSMetaDataStore.h"
#include "MetaDataStoreInterface.h"
#include "RelocationReaderFactory.h"
#include "SCOAccessData.h"
#include "Scrubber.h"
#include "SnapshotManagement.h"
#include "TracePoints_tp.h"
#include "TransientException.h"
#include "TLogReader.h"
#include "VolManager.h"
#include "Volume.h"
#include "ScrubReply.h"
#include "ScrubWork.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include <math.h>
#include <float.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/chrono.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/scope_exit.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/FileUtils.h>
#include <youtils/IOException.h>
#include <youtils/MainEvent.h>
#include <youtils/ScopeExit.h>
#include <youtils/Timer.h>
#include <youtils/System.h>
#include <youtils/UUID.h>

#include <backend/BackendInterface.h>
#include <backend/Garbage.h>
#include <backend/GarbageCollector.h>

// sanity check
static_assert(FLT_RADIX == 2, "Need to check code for non conforming FLT_RADIX");

#define WLOCK()                                 \
    boost::unique_lock<decltype(rwlock_)> urwlg__(rwlock_)

#define RLOCK()                                 \
    boost::shared_lock<decltype(rwlock_)> srwlg__(rwlock_)

#define SERIALIZE_WRITES()                              \
    boost::lock_guard<lock_type> gwl__(write_lock_)

#define LOCK_CONFIG()                           \
    std::lock_guard<decltype(config_lock_)> gcfglck__(config_lock_)

#ifndef NDEBUG

#define ASSERT_WLOCKED()                        \
    VERIFY(not rwlock_.try_lock_shared())

#define ASSERT_RLOCKED()                        \
    VERIFY(not rwlock_.try_lock())

#define ASSERT_WRITES_SERIALIZED()              \
    VERIFY(not write_lock_.try_lock())

#else

#define ASSERT_WLOCKED()
#define ASSERT_RLOCKED()
#define ASSERT_WRITES_SERIALIZED()

#endif

namespace volumedriver
{
using youtils::BarrierTask;

namespace bc = boost::chrono;
namespace be = backend;
namespace yt = youtils;

Volume::Volume(const VolumeConfig& vCfg,
               const OwnerTag owner_tag,
               std::unique_ptr<SnapshotManagement> snapshotManagement,
               std::unique_ptr<DataStoreNG> datastore,
               std::unique_ptr<MetaDataStoreInterface> metadatastore,
               NSIDMap nsidmap,
               const std::atomic<unsigned>& foc_throttle_usecs,
               bool& readOnlyMode)
    : mdstore_was_rebuilt_(false)
    , config_lock_()
    , write_lock_()
    , rwlock_("rwlock-" + vCfg.id_.str())
    , halted_(false)
    , dataStore_(datastore.release())
    , failover_(FailOverCacheClientInterface::create(FailOverCacheMode::Asynchronous,
                                                     LBASize(vCfg.lba_size_),
                                                     vCfg.cluster_mult_,
                                                     VolManager::get()->dtl_queue_depth.value(),
                                                     VolManager::get()->dtl_write_trigger.value()))
    , metaDataStore_(metadatastore.release())
    , clusterSize_(vCfg.getClusterSize())
    , config_(vCfg)
    , snapshotManagement_(snapshotManagement.release())
    , volOffset_(ilogb(vCfg.lba_size_ ))
    , nsidmap_(std::move(nsidmap))
    , failoverstate_(VolumeFailOverState::DEGRADED)
    , readcounter_(0)
    , read_activity_(0)
    , cluster_locations_(vCfg.sco_mult_)
    , volumeStateSpinLock_()
    , readOnlyMode(readOnlyMode)
    , datastore_throttle_usecs_(VolManager::get()->getSCOCache()->datastore_throttle_usecs.value())
    , foc_throttle_usecs_(foc_throttle_usecs)
    , readCacheHits_(0)
    , readCacheMisses_(0)
    , has_dumped_debug_data(false)
    , number_of_syncs_(0)
    , total_number_of_syncs_(0)
{
    WLOCK();

    LOG_VINFO("Constructor of volume, namespace " << vCfg.getNS());

    maybe_update_owner_tag_(owner_tag);

    VERIFY(vCfg.lba_size_ == exp2(volOffset_));

    caMask_ = ~(getClusterSize() / getLBASize() - 1);
    dataStore_->initialize(this);
    snapshotManagement_->initialize(this);
    init_failover_cache_();

    VERIFY(metaDataStore_->scrub_id() != boost::none);
    VERIFY(*metaDataStore_->scrub_id() == snapshotManagement_->scrub_id());

    metaDataStore_->initialize(*this);
    metaDataBackendConfigHasChanged(*metaDataStore_->getBackendConfig());

    prefetch_data_.initialize(this);
    prefetch_thread_.reset(new boost::thread(boost::ref(prefetch_data_)));
}

Volume::~Volume()
{
    WLOCK();

    LOG_VINFO("Destructor of " << this);
    VolManager::get()->backend_thread_pool()->stop(this);

    // we really shouldn't end up here with a running prefetch_thread_ - destroy()
    // is supposed to have stopped it already.
    ASSERT(prefetch_thread_ == nullptr);

    try
    {
        stopPrefetch_();
    }
    CATCH_STD_ALL_VLOG_IGNORE("Exception stopping the prefetch thread");
}

void
Volume::maybe_update_owner_tag_(const OwnerTag owner_tag)
{
    const OwnerTag old = getOwnerTag();
    if (old != owner_tag)
    {
        LOG_VINFO("Owner Tag has changed from " << old << " to " <<
                  owner_tag);
        config_.owner_tag_ = owner_tag;
        reregister_with_cluster_cache_(old,
                                       owner_tag);
    }
}

void
Volume::setSCOMultiplier(SCOMultiplier sco_mult)
{
    SERIALIZE_WRITES();
    WLOCK();

    checkNotHalted_();

    const size_t csize = getClusterSize();
    const uint64_t sco_size_new = csize * sco_mult;
    static const uint64_t sco_size_min =
        std::min((1ULL << (8 * sizeof(SCOOffset))) * csize,
                 yt::System::get_env_with_default("VOLUMEDRIVER_MIN_SCO_SIZE",
                                                  1ULL << 20)); // 1 MiB
    static const uint64_t sco_size_max =
        std::min((1ULL << (8 * sizeof(SCOOffset))) * csize,
                 yt::System::get_env_with_default("VOLUMEDRIVER_MAX_SCO_SIZE",
                                                  128ULL << 20)); // 128 MiB

    if (sco_size_new < sco_size_min)
    {
        LOG_VERROR("invalid SCO multiplier " << sco_mult << ", SCO sizes < " <<
                   sco_size_min << " are not supported");
        throw InvalidOperation("Resulting SCO size is too small");
    }
    else if (sco_size_new > sco_size_max)
    {
        LOG_VERROR("invalid SCO multiplier " << sco_mult << ", SCO sizes > " <<
                   sco_size_max << " are not supported");
        throw InvalidOperation("Resulting SCO size is too big");
    }

    const boost::optional<TLogMultiplier> tlog_mult(getTLogMultiplier());

    VolManager::get()->checkSCOAndTLogMultipliers(getClusterSize(),
                                                  getSCOMultiplier(),
                                                  sco_mult,
                                                  tlog_mult,
                                                  tlog_mult);

    cluster_locations_.resize(sco_mult);
    dataStore_->setSCOMultiplier(sco_mult);
    snapshotManagement_->set_max_tlog_entries(tlog_mult,
                                              sco_mult);

    // force SCO (and TLog, but we care about the former) rollover
    scheduleBackendSync_();

    update_config_([this,
                    sco_mult](VolumeConfig& cfg)
                   {
                       cfg.sco_mult_ = sco_mult;
                   });
}

void
Volume::setTLogMultiplier(const boost::optional<TLogMultiplier>& tlog_mult)
{
    SERIALIZE_WRITES();
    WLOCK();

    checkNotHalted_();

    if (tlog_mult and *tlog_mult == 0)
    {
        LOG_VERROR("invalid new TLogMultiplier 0");
        throw InvalidOperation("TLOG multiplier must be > 0");
    }

    const SCOMultiplier sco_mult(getSCOMultiplier());

    VolManager::get()->checkSCOAndTLogMultipliers(getClusterSize(),
                                                  sco_mult,
                                                  sco_mult,
                                                  getTLogMultiplier(),
                                                  tlog_mult);

    snapshotManagement_->set_max_tlog_entries(tlog_mult,
                                              sco_mult);

    update_config_([this,
                    &tlog_mult](VolumeConfig& cfg)
                   {
                       cfg.tlog_mult_ = tlog_mult;
                   });
}

void
Volume::setSCOCacheMaxNonDisposableFactor(const boost::optional<SCOCacheNonDisposableFactor>& max_non_disposable_factor)
{
    update_config_([this,
                    max_non_disposable_factor](VolumeConfig& cfg)
    {
        if (max_non_disposable_factor)
        {
            // bug in OUR_STRONG_ARITHMETIC_TYPEDEF for float
            float mndf = max_non_disposable_factor.get();
            if (mndf < 1.0)
            {
                throw InvalidOperation("Max non disposable factor must be >= 1.0");
            }
        }
        cfg.max_non_disposable_factor_ = max_non_disposable_factor;
        setSCOCacheLimitMax(VolManager::get()->get_sco_cache_max_non_disposable_bytes(cfg),
                            cfg.getNS());
    });
}

void
Volume::setSyncIgnore(uint64_t number_of_syncs_to_ignore,
                      uint64_t maximum_time_to_ignore_syncs_in_seconds)
{

    update_config_([number_of_syncs_to_ignore,
                    maximum_time_to_ignore_syncs_in_seconds](VolumeConfig& cfg)
                   {
                       cfg.number_of_syncs_to_ignore_ = number_of_syncs_to_ignore;
                       cfg.maximum_time_to_ignore_syncs_in_seconds_ = maximum_time_to_ignore_syncs_in_seconds;
                   });
}

void
Volume::getSyncIgnore(uint64_t& number_of_syncs_to_ignore,
              uint64_t& maximum_time_to_ignore_syncs_in_seconds) const
{
    LOCK_CONFIG();
    number_of_syncs_to_ignore = config_.number_of_syncs_to_ignore_;
    maximum_time_to_ignore_syncs_in_seconds = config_.maximum_time_to_ignore_syncs_in_seconds_;
}

void
Volume::metaDataBackendConfigHasChanged(const MetaDataBackendConfig& mcfg)
{
    update_config_([&](VolumeConfig& vcfg)
                   {
                       vcfg.metadata_backend_config_ = mcfg.clone();
                   });
}

void
Volume::update_config_(const UpdateFun& fun)
{
    VolumeConfig cfg;

    {
        LOCK_CONFIG();
        TODO("AR: revisit: might be too expensive for a spinlock?");
        // ... OTOH: do we care?
        fun(config_);
        cfg = config_;
    }

    writeConfigToBackend_(cfg);
}

// called from mgmt thread
void
Volume::localRestartDataStore_(SCONumber last_sco_num_in_backend,
                               ClusterLocation last_loc)
{
    ASSERT_WLOCKED();

    SCOAccessDataPtr sad;

    const VolumeConfig cfg(get_config());

    try
    {
        BackendInterfacePtr bi(VolManager::get()->createBackendInterface(cfg.getNS()));
        SCOAccessDataPersistor sadp(std::move(bi));
        sad = sadp.pull();
    }
    catch (std::exception& e)
    {
        LOG_VINFO("could not get SCO access data for namespace " << cfg.getNS() << ": " <<
                  e.what() << " - ignoring");
        sad.reset(new SCOAccessData(cfg.getNS())); // empty SCOAccessData
    }

    const uint64_t max_non_disposable =
        VolManager::get()->get_sco_cache_max_non_disposable_bytes(cfg);

    const ClusterLocation loc(dataStore_->localRestart(0,
                                                       max_non_disposable,
                                                       *sad,
                                                       last_sco_num_in_backend,
                                                       last_loc));


    LOG_VINFO("Local restart cluster location is " << loc);

    SCONameList nondisposables;
    VolManager::get()->getSCOCache()->getSCONameList(getNamespace(),
                                                     nondisposables,
                                                     false);

    {
        OrderedTLogIds tlogs;
        snapshotManagement_->getTLogsNotWrittenToBackend(tlogs);
        // explicitly say no fetch from backend should be done by not passing in a
        // backend pointer
        std::shared_ptr<TLogReaderInterface>
            reader(CombinedTLogReader::create(snapshotManagement_->getTLogsPath(),
                                              tlogs,
                                              nullptr));

        std::map<SCO, CheckSum> checksums;
        reader->SCONamesAndChecksums(checksums);

        for (const auto& sco_and_checksum : checksums)
        {
            const SCO sco = sco_and_checksum.first;
            const CheckSum checksum(sco_and_checksum.second);
            dataStore_->getAndPushSCOForLocalRestart(sco,
                                                     checksum);
            nondisposables.remove(sco);
        }
    }

    if (not nondisposables.empty())
    {
        LOG_VERROR("Some nondisposables left over that are not in the TLogs. Should not happen, please report a bug.");

        for (const SCO& sco : nondisposables)
        {
            LOG_VERROR("Removing left over SCO " << sco);
            try
            {
                dataStore_->removeSCO(sco, true);
            }
            CATCH_STD_ALL_VLOG_IGNORE("Exception removing leftover sco " << sco);
        }
    }
}

void
Volume::localRestart()
{
    WLOCK();

    const VolumeConfig cfg(get_config());

    const ClusterLocation last_loc_in_backend(snapshotManagement_->getLastSCOInBackend());
    LOG_VINFO("Last SCO:offset in backend: " << last_loc_in_backend);

    const ClusterLocation last_loc(snapshotManagement_->getLastSCO());
    LOG_VINFO("Last SCO:offset overall: " << last_loc);

    //implicitly also checks (lastSCO.isSentinel => lastSCOInBackend.isSentinel)  (=> being logical implication)
    VERIFY(last_loc_in_backend.number() <= last_loc.number());

    localRestartDataStore_(last_loc_in_backend.number(),
                           last_loc);

    LOG_VINFO("getting the failover information from the backend");

    foc_config_wrapper_ = *VolManager::get()->get_config_from_backend<FailOverCacheConfigWrapper>(cfg.getNS());

    if (foc_config_wrapper_.config())
    {
        FailOverCacheConfig foc_cfg = foc_config_wrapper_.config().get();
        std::unique_ptr<FailOverCacheProxy> cache;

        try
        {
            cache =
                std::make_unique<FailOverCacheProxy>(foc_cfg,
                                                     cfg.getNS(),
                                                     TODO("AR: fix getLBASize instead")
                                                     LBASize(getLBASize()),
                                                     getClusterMultiplier(),
                                                     failover_->getDefaultRequestTimeout());
        }
        CATCH_STD_ALL_EWHAT({
                LOG_VINFO("Could not start with previous failover settings: " << EWHAT);
                setVolumeFailOverState(VolumeFailOverState::DEGRADED);
            });

        if(cache)
        {
            setFailOverCacheMode_(foc_cfg.mode);
            failover_->newCache(std::move(cache));
            setVolumeFailOverState(VolumeFailOverState::OK_SYNC);
        }
    }
    else
    {
        setVolumeFailOverState(VolumeFailOverState::OK_STANDALONE);
    }

    snapshotManagement_->scheduleTLogsToBeWrittenToBackend();
    if (snapshotManagement_->currentTLogHasData())
    {
        MaybeCheckSum cs;
        snapshotManagement_->scheduleBackendSync(cs);
    }

    snapshotManagement_->scheduleWriteSnapshotToBackend();

    register_with_cluster_cache_(getOwnerTag());
}

void
Volume::check_cork_match_()
{
#ifndef NDEBUG
    LOG_VINFO("Snapshot Cork: " << snapshotManagement_->lastCork());
    LOG_VINFO("Metadatastore Cork: " << metaDataStore_->lastCork());
#endif

    if(not snapshotManagement_->parent() or
       snapshotManagement_->lastCork() != boost::none)
    {
        VERIFY(snapshotManagement_->lastCork() == metaDataStore_->lastCork());
    }
}

// called from mgmt thread
Volume*
Volume::newVolume()
{
    WLOCK();

    const VolumeConfig cfg(get_config());

    const uint64_t max_non_disposable =
        VolManager::get()->get_sco_cache_max_non_disposable_bytes(cfg);

    dataStore_->newVolume(0,
                          max_non_disposable);

    check_cork_match_();

    snapshotManagement_->scheduleWriteSnapshotToBackend();
    writeConfigToBackend_(cfg);
    writeFailOverCacheConfigToBackend_();
    setVolumeFailOverState(VolumeFailOverState::OK_STANDALONE);

    register_with_cluster_cache_(getOwnerTag());
    return this;
}

// called from mgmt thread
Volume*
Volume::backend_restart(const CloneTLogs& restartTLogs,
                        const SCONumber lastSCOInBackend,
                        const IgnoreFOCIfUnreachable ignoreFOCIfUnreachable,
                        const boost::optional<yt::UUID>& last_snapshot_cork)
{
    SERIALIZE_WRITES();
    WLOCK();

    const VolumeConfig cfg(get_config());

    const uint64_t max_non_disposable =
        VolManager::get()->get_sco_cache_max_non_disposable_bytes(cfg);

    dataStore_->backend_restart(lastSCOInBackend,
                                0,
                                max_non_disposable);

    VERIFY(restartTLogs.size() <= nsidmap_.size());
    // We sync the metadastore here so we have the cork of the last tlog in
    // backend as restart point
    metaDataStore_->processCloneTLogs(restartTLogs,
                                      nsidmap_,
                                      VolManager::get()->getTLogPath(cfg),
                                      true,
                                      last_snapshot_cork);

    check_cork_match_();

    const_cast<bool&>(mdstore_was_rebuilt_) = true;
    LOG_VINFO("getting the failover information from the backend");

    foc_config_wrapper_ =
        *VolManager::get()->get_config_from_backend<FailOverCacheConfigWrapper>(cfg.getNS());

    VolumeFailOverState foc_state = VolumeFailOverState::OK_STANDALONE;

    if (foc_config_wrapper_.config())
    {
        FailOverCacheConfig foc_cfg = foc_config_wrapper_.config().get();
        std::unique_ptr<FailOverCacheProxy> cache;

        try
        {
            cache =
                std::make_unique<FailOverCacheProxy>(foc_cfg,
                                                     cfg.getNS(),
                                                     LBASize(getLBASize()),
                                                     getClusterMultiplier(),
                                                     failover_->getDefaultRequestTimeout());
        }
        CATCH_STD_ALL_EWHAT({
                if (ignoreFOCIfUnreachable == IgnoreFOCIfUnreachable::T)
                {
                    LOG_VWARN("we had a FailoverCache configured but it is currently unreachable: " << EWHAT << " - Continuing without as requested.");
                    foc_state = VolumeFailOverState::DEGRADED;
                }
                else
                {
                    LOG_VERROR("we had a FailoverCache configured but it is currently unreachable: " << EWHAT);
                    throw;
                }
            });

        //not within the try as we don't want to ignore problems while replaying
        if (cache.get())
        {
            SCO oldest, youngest;
            cache->getSCORange(oldest,youngest);
            const SCONumber old = oldest.number();
            const SCONumber young = youngest.number();

            LOG_VINFO("Oldest SCO on FOC " << old << " youngest SCO on FOC " <<
                      young);

            foc_state = VolumeFailOverState::OK_SYNC;

            if(not oldest.asBool())
            {
                VERIFY(not youngest.asBool());
                LOG_VINFO("FailoverCache does not contain data");
            }
            else if(lastSCOInBackend + 1 >= old and
                    young > lastSCOInBackend)
            {
                LOG_VINFO("FailoverCache contains data and will be replayed from SCONumber " << old);
                if(lastSCOInBackend != 0)
                {
                   cache->removeUpTo(ClusterLocation(lastSCOInBackend).sco());
                }

                replayFOC_(*cache);
            }
            else
            {
                LOG_VINFO("FailoverCache contains no useable data, since lastSCOInBackend: " <<
                          lastSCOInBackend);
                try
                {
                    cache->clear();
                }
                CATCH_STD_ALL_EWHAT({
                        LOG_VERROR("failed to clear FailOverCache: " << EWHAT);
                        if (ignoreFOCIfUnreachable == IgnoreFOCIfUnreachable::T)
                        {
                            foc_state = VolumeFailOverState::DEGRADED;
                        }
                        else
                        {
                            throw;
                        }
                    });
            }
        }

        setFailOverCacheMode_(foc_cfg.mode);
        failover_->newCache(std::move(cache));
    }

    setVolumeFailOverState(foc_state);
    snapshotManagement_->scheduleWriteSnapshotToBackend();
    register_with_cluster_cache_(getOwnerTag());

    return this;
}

// Y42 redo these, they should just be a right shift of lba by an amount
ClusterAddress Volume::addr2CA(const uint64_t addr) const
{
    return addr / getClusterSize();
}

uint64_t Volume::LBA2Addr(const uint64_t lba) const
{
    return (lba & caMask_) * getLBASize();
}

void
Volume::validateIOLength(uint64_t lba, uint64_t len) const
{
    if (lba + len / getLBASize() > getLBACount())
    {
        LOG_VWARN("Access beyond end of volume: LBA "
                 << (lba + len / getLBASize()) << " > "
                 << getLBACount());
        // don't throw return boolean!!
        throw fungi::IOException("Access beyond end of volume");
    }
}

void
Volume::validateIOAlignment(uint64_t lba, uint64_t len) const
{
    if (len % getClusterSize())
    {
        LOG_VWARN("Access not aligned to cluster size: len " << len <<
                 ", clustersize " << getClusterSize());
        throw fungi::IOException("Access not aligned to cluster size");
    }
    if (lba & ~caMask_)
    {
        LOG_VWARN("LBA " << lba << " not aligned to cluster (mask: " <<
                 std::hex << ~caMask_ << std::dec);
        throw fungi::IOException("LBA not aligned to cluster");
    }
}

void
Volume::writeClusterMetaData_(ClusterAddress ca,
                              const ClusterLocationAndHash& loc_and_hash)
{
    ASSERT_WRITES_SERIALIZED();
    ASSERT_RLOCKED();

    snapshotManagement_->addClusterEntry(ca,
                                         loc_and_hash);
    try
    {
        metaDataStore_->writeCluster(ca, loc_and_hash);
    }
    CATCH_STD_ALL_EWHAT({
            VolumeDriverError::report(events::VolumeDriverErrorCode::MetaDataStore,
                                      EWHAT,
                                      getName());
            halt();
            throw;
        });
}

void
Volume::setAsTemplate()
{
    WLOCK();
    checkNotHalted_();

    const VolumeConfig cfg(get_config());

    sync_(AppendCheckSum::F);

    const MaybeCheckSum maybe_sco_crc(dataStore_->finalizeCurrentSCO());

    snapshotManagement_->setAsTemplate(maybe_sco_crc);

    update_config_([&](VolumeConfig& cfg)
                   {
                       cfg.setAsTemplate();
                   });

    sync_(AppendCheckSum::T);

    while (not VolManager::get()->backend_thread_pool()->noTasksPresent(this))
    {
        sleep(1);
    }
}

void
Volume::writeClustersToFailOverCache_(const std::vector<ClusterLocation>& locs,
                                      size_t num_locs,
                                      uint64_t start_address,
                                      const uint8_t* buf)
{
    ASSERT_WRITES_SERIALIZED();
    ASSERT_RLOCKED();

    if (failover_->backup())
    {
        tracepoint(openvstorage_volumedriver,
                   volume_foc_write_start,
                   config_.id_.str().c_str(),
                   start_address,
                   reinterpret_cast<const uint64_t&>(locs[0]));

        auto on_exit(yt::make_scope_exit([&]
        {
            tracepoint(openvstorage_volumedriver,
                       volume_foc_write_end,
                       config_.id_.str().c_str(),
                       start_address,
                       reinterpret_cast<const uint64_t&>(locs[0]),
                       std::uncaught_exception());
        }));

        LOG_VTRACE("sending TLog " << snapshotManagement_->getCurrentTLogId() <<
                   ", start address " << start_address);

        while (not failover_->addEntries(locs,
                                         num_locs,
                                         start_address,
                                         buf))
        {
            throttle_(foc_throttle_usecs_);
        }
    }
}

void
Volume::write(uint64_t lba,
              const uint8_t *buf,
              uint64_t buflen)
{
    tracepoint(openvstorage_volumedriver,
               volume_write_start,
               config_.id_.str().c_str(),
               lba,
               buflen);

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         tracepoint(openvstorage_volumedriver,
                                                    volume_write_end,
                                                    config_.id_.str().c_str(),
                                                    lba,
                                                    buflen,
                                                    std::uncaught_exception());
                                     }));

    performance_counters().write_request_size.count(buflen);

    if (VolManager::get()->volume_nullio.value())
    {
        return;
    }

    yt::SteadyTimer t;

    if (T(isVolumeTemplate()))
    {
        LOG_ERROR("Volume " << getName() << " has been templated, write is not allowed.");
        throw VolumeIsTemplateException("Templated Volume, write forbidden");
    }

    LOG_VTRACE("lba " << lba << ", len " << buflen << ", buf " << &buf);

    checkNotHalted_();
    checkNotReadOnly_();

    //MeasureScopeInt ms("Volume::write", lba);
    validateIOLength(lba, buflen);

    uint64_t addrOffset = (lba & ~caMask_) * getLBASize();
    uint64_t len = intCeiling(addrOffset + buflen, getClusterSize());

    bool unaligned = (lba & ~caMask_) != 0 ||
                     buflen % getClusterSize() != 0;

    std::vector<uint8_t> bufv;
    const uint8_t *p;
    uint64_t alignedLBA = lba & caMask_;

    if (unaligned)
    {
        performance_counters().unaligned_write_request_size.count(buflen);

        validateIOAlignment(alignedLBA, len);
        bufv.resize(len);
        p = &bufv[0];
        read(alignedLBA, &bufv[0], len);
        LOG_VDEBUG("Unaligned write: lba " << lba << ", len " <<
                   buflen << " -> using bounce buffer " << &p <<
                   " for lba " << alignedLBA << ", len " << len);
        memcpy(&bufv[0] + addrOffset, buf, buflen);
    }
    else
    {
        p = buf;
    }

    // addr: closest cluster boundary in bytes
    uint64_t addr = LBA2Addr(lba);
    size_t wsize = len;
    size_t off = 0;

    while (off < len)
    {
        SERIALIZE_WRITES();
        const ssize_t sco_cap =
            dataStore_->getRemainingSCOCapacity() * getClusterSize();
        // we need to guarantee that ourselves by forcing a SCO rollover
        // when updating the SCOMultiplier
        VERIFY(sco_cap >= 0); // can we really deal with sco_cap == 0?

        size_t dtl_cap = std::numeric_limits<size_t>::max();
        {
            RLOCK();
            if (failover_)
            {
                dtl_cap = failover_->max_entries() * getClusterSize();
            }
        }

        VERIFY(dtl_cap > 0);

        const size_t chunksize =
            std::min({wsize,
                      dtl_cap,
                      static_cast<size_t>(sco_cap)});

        writeClusters_(addr + off, p + off, chunksize);
        off += chunksize;
        wsize -= chunksize;
    }

    const auto duration_us(bc::duration_cast<bc::microseconds>(t.elapsed()));
    performance_counters().write_request_usecs.count(duration_us.count());

    VERIFY(off == len);
}

namespace
{
ClusterLocationAndHash
make_cluster_location_and_hash(const ClusterLocation& loc,
                               const ClusterCacheMode ccmode,
                               const uint8_t* buf,
                               const size_t bufsize)
{
    if (ccmode == ClusterCacheMode::ContentBased)
    {
        return ClusterLocationAndHash(loc,
                                      buf,
                                      bufsize);
    }
    else
    {
        return ClusterLocationAndHash(loc,
                                      yt::Weed::null());
    }
}

}

void
Volume::writeClusters_(uint64_t addr,
                       const uint8_t* buf,
                       uint64_t bufsize)
{
    ASSERT_WRITES_SERIALIZED();

    VERIFY(bufsize % getClusterSize() == 0);

    size_t num_locs = bufsize / getClusterSize();
    unsigned throttle_usecs = 0;
    uint32_t ds_throttle = 0;

    {
        // prevent tlog rollover interfering with snapshotting and friends
        RLOCK();

        TODO("ArneT: reserve/clear vector \"cluster_locations\" so the nr of clusters can be derived in the failover_->addEntries call");

        dataStore_->writeClusters(buf,
                                  cluster_locations_,
                                  num_locs,
                                  ds_throttle);

        const ClusterCacheMode ccmode = effective_cluster_cache_mode();

        for (size_t i = 0; i < num_locs; ++i)
        {
            const uint8_t* data = buf + i * getClusterSize();
            uint64_t clusteraddr = addr + i * getClusterSize();
            ClusterAddress ca = addr2CA(clusteraddr);
            ClusterLocationAndHash
                loc_and_hash(make_cluster_location_and_hash(cluster_locations_[i],
                                                            ccmode,
                                                            data,
                                                            getClusterSize()));

            writeClusterMetaData_(ca,
                                  loc_and_hash);

            if (isCacheOnWrite())
            {
                add_to_cluster_cache_(ccmode,
                                      ca,
                                      loc_and_hash.weed(),
                                      data);
            }
            else if (ccmode == ClusterCacheMode::LocationBased)
            {
                purge_from_cluster_cache_(ca,
                                          loc_and_hash.weed());
            }
        }

        yt::SteadyTimer t;
        writeClustersToFailOverCache_(cluster_locations_,
                                      num_locs,
                                      addr >> volOffset_,
                                      buf);

        throttle_usecs += bc::duration_cast<bc::microseconds>(t.elapsed()).count();

        const ssize_t sco_cap = dataStore_->getRemainingSCOCapacity();
        VERIFY(sco_cap >= 0);

        if (sco_cap == 0)
        {
            LOG_VDEBUG(getName() << ": requesting SCO rollover, adding CRC to tlog");
            MaybeCheckSum cs = dataStore_->finalizeCurrentSCO();
            VERIFY(cs);
            snapshotManagement_->addSCOCRC(*cs);
        }
    }

    LOG_VTRACE("start_address " << addr <<
               " CA " << cluster_locations_[0]);

    if (ds_throttle > 0)
    {
        const unsigned ds_throttle_usecs = ds_throttle * num_locs;
        throttle_usecs =
            ds_throttle_usecs - std::min(ds_throttle_usecs,
                                         throttle_usecs);

        throttle_(throttle_usecs);
    }
}

void
Volume::read(uint64_t lba,
             uint8_t *buf,
             uint64_t buflen)
{
    tracepoint(openvstorage_volumedriver,
               volume_read_start,
               config_.id_.str().c_str(),
               lba,
               buflen);

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         tracepoint(openvstorage_volumedriver,
                                                    volume_read_end,
                                                    config_.id_.str().c_str(),
                                                    lba,
                                                    buflen,
                                                    std::uncaught_exception());
                                     }));

    performance_counters().read_request_size.count(buflen);

    if (VolManager::get()->volume_nullio.value())
    {
        return;
    }

    yt::SteadyTimer t;

    LOG_VTRACE("lba " << lba << ", len " << buflen << ", buf " << &buf);

    checkNotHalted_();

    validateIOLength(lba, buflen);

    bool unaligned = (lba & ~caMask_) ? true : false;
    uint64_t len;
    if (unaligned || buflen % getClusterSize())
    {
        unaligned = true;
        len = (lba & ~caMask_) * getLBASize() + buflen;
        if (len % getClusterSize())
            len = (len / getClusterSize() + 1) * getClusterSize();
    }
    else
    {
        len = buflen;
    }

    std::vector<uint8_t> bufv;
    uint8_t *p;

    if (unaligned)
    {
        performance_counters().unaligned_read_request_size.count(buflen);

        validateIOAlignment(lba & caMask_, len);
        bufv.resize(len);
        p = &bufv[0];
    }
    else
    {
        p = buf;
    }
    uint64_t addr = LBA2Addr(lba);

    readClusters_(addr,
                  p,
                  len);

    if (unaligned)
    {
        LOG_VDEBUG("Unaligned read: lba " << lba << ", len " <<
                   buflen << " -> using bounce buffer " << &p <<
                   " for lba " << (lba & caMask_) << ", len " << len);
        memcpy(buf, p + (lba & ~caMask_) * getLBASize(), buflen);
    }

    const auto duration_us(bc::duration_cast<bc::microseconds>(t.elapsed()));
    performance_counters().read_request_usecs.count(duration_us.count());
}

void
Volume::readClusters_(uint64_t addr,
                      uint8_t* buf,
                      uint64_t bufsize)
{
    RLOCK();

    // Z42: Move this to the caller or make it thread local storage to
    // avoid allocations
    std::vector<ClusterReadDescriptor> read_descriptors;
    read_descriptors.reserve(bufsize / getClusterSize());
    const ClusterCacheMode ccmode = effective_cluster_cache_mode();

    for (uint64_t off = 0; off < bufsize; off += getClusterSize())
    {
        TODO("AR: go to the cluster cache immediately when LocationBased?");

        ClusterLocationAndHash loc_and_hash;
        ClusterAddress ca = addr2CA(addr + off);
        ++readcounter_;

        try
        {
            metaDataStore_->readCluster(ca, loc_and_hash);
        }
        CATCH_STD_ALL_EWHAT({
                VolumeDriverError::report(events::VolumeDriverErrorCode::MetaDataStore,
                                          EWHAT,
                                          getName());
                halt();
                throw;
            });

        LOG_VTRACE("lba " << ((addr + off) / getLBASize()) <<
                   " CA " << loc_and_hash);

        if (loc_and_hash.clusterLocation.isNull())
        {
            // read of cluster that has not been written
            // TRACE?
            LOG_VTRACE("read lba " << addr / getLBASize() << " + " << bufsize <<
                       ", off " << off << ", CA " <<
                       addr2CA(addr + off) <<
                       " - CA not previously written to");
            memset(buf + off, 0x0, getClusterSize());
        }
        else
        {
            const bool in_cache = find_in_cluster_cache_(ccmode,
                                                         ca,
                                                         loc_and_hash.weed(),
                                                         buf + off);

            if (in_cache)
            {
                ++readCacheHits_;
                dataStore_->touchCluster(loc_and_hash.clusterLocation);
            }
            else
            {
                ++readCacheMisses_;
                read_descriptors.
                    push_back(ClusterReadDescriptor(loc_and_hash,
                                                    ca,
                                                    buf + off,
                                                    getBackendInterface(loc_and_hash.clusterLocation.cloneID())->clone()));
            }
        }
    }

    dataStore_->readClusters(read_descriptors);

    if (effective_cluster_cache_behaviour() != ClusterCacheBehaviour::NoCache)
    {
        for (const ClusterReadDescriptor& clrd : read_descriptors)
        {
            add_to_cluster_cache_(ccmode,
                                  clrd.getClusterAddress(),
                                  clrd.weed(),
                                  clrd.getBuffer());
        }
    }
}

std::tuple<uint64_t, uint64_t>
Volume::getSyncSettings()
{
    LOCK_CONFIG();
    return std::make_tuple(config_.number_of_syncs_to_ignore_,
                           config_.maximum_time_to_ignore_syncs_in_seconds_);
}

uint64_t
Volume::getSize() const
{
    LOCK_CONFIG();
    return config_.lba_size_ * config_.lba_count();
}

uint64_t
Volume::getLBASize() const
{
    LOCK_CONFIG();
    return config_.lba_size_;
}

uint64_t
Volume::getLBACount() const
{
    /* atomic */
    return config_.lba_count().load();
}

const VolumeId
Volume::getName() const
{
    LOCK_CONFIG();
    return config_.id_;
}

backend::Namespace
Volume::getNamespace() const
{
    LOCK_CONFIG();
    return config_.getNS();
}

VolumeConfig
Volume::get_config() const
{
    LOCK_CONFIG();
    return config_;
}

boost::optional<FailOverCacheConfig>
Volume::getFailOverCacheConfig()
{
    return foc_config_wrapper_.config();
}

void
Volume::createSnapshot(const SnapshotName& name,
                       const SnapshotMetaData& metadata,
                       const UUID& uuid)
{
    if(T(isVolumeTemplate()))
    {
        LOG_ERROR("Volume " << getName() << " has been templated, creating snapshots is not allowed.");
        throw VolumeIsTemplateException("Templated Volume, snapshot creation forbidden");
    }

    WLOCK(); // Z42: make more fine grained?

    checkNotHalted_();

    if (not snapshotManagement_->lastSnapshotOnBackend())
    {
        LOG_VERROR("Last snapshot not on backend");
        throw PreviousSnapshotNotOnBackendException("Last snapshot not on backend, can't create a new one");
    }

    // TODO: halt the volume in case of non-recoverable errors
    sync_(AppendCheckSum::F);

    const auto maybe_sco_crc(dataStore_->finalizeCurrentSCO());
    snapshotManagement_->createSnapshot(name,
                                        maybe_sco_crc,
                                        metadata,
                                        uuid);
}

bool
Volume::checkSnapshotUUID(const SnapshotName& snapshotName,
                          const volumedriver::UUID& uuid) const
{
    return snapshotManagement_->checkSnapshotUUID(snapshotName,
                                                  uuid);
}

bool
Volume::snapshotExists(const SnapshotName& name) const
{
    return snapshotManagement_->snapshotExists(name);
}

void
Volume::listSnapshots(std::list<SnapshotName>& snapshots) const
{
    snapshotManagement_->listSnapshots(snapshots);
}

Snapshot
Volume::getSnapshot(const SnapshotName& snapname) const
{
    return snapshotManagement_->getSnapshot(snapname);
}

void Volume::deleteSnapshot(const SnapshotName& name)
{
    if(T(isVolumeTemplate()))
    {
        LOG_ERROR("Volume " << getName() << " has been templated, deleting snapshot is not allowed.");
        throw VolumeIsTemplateException("Templated Volume, deleting snapshot work is forbidden");
    }

    checkNotHalted_();
    snapshotManagement_->deleteSnapshot(name);
}

fs::path
Volume::ensureDebugDataDirectory_()
{
    fs::path directory = VolManager::get()->debug_metadata_path.value();
    fs::create_directories(directory);

    std::stringstream ss;
    ss << time(0);

    std::string filename = getName() + "_debugdata_for_halted_volume_" + ss.str() + ".tar.gz";

    return directory / filename;
}

void
Volume::dumpDebugData()
{
    if(not has_dumped_debug_data)
    {
        try
        {
            const fs::path path = ensureDebugDataDirectory_();
            const fs::path global_metadata_directory_parent = VolManager::get()->getMetaDataPath().parent_path();
            const fs::path relative_metadata_directory = VolManager::get()->getMetaDataPath().filename();
            const fs::path global_tlog_directory_parent = VolManager::get()->getTLogPath().parent_path();
            const fs::path relative_tlog_directory = VolManager::get()->getTLogPath().filename();

            std::stringstream ss;
            ss << "tar -zcf " << path.string()
               << " -C " << global_metadata_directory_parent.string() << " "
               << (relative_metadata_directory / fs::path(getNamespace().str())).string()
               << " -C " << global_tlog_directory_parent.string() << " "
               << (relative_tlog_directory / fs::path(getNamespace().str())).string();

            has_dumped_debug_data = youtils::System::system_command(ss.str());
        }
        catch(std::exception& e)
        {
            LOG_VWARN("Could not dump volume debug data, " << e.what());
        }
        catch(...)
        {
            LOG_VWARN("Could not dump volume debug data, unknown exception");
        }
    }
}

// TODO: this only works when the backend is online
void
Volume::restoreSnapshot(const SnapshotName& name)
{
    WLOCK();

    const VolumeConfig cfg(get_config());
    const SnapshotNum num = snapshotManagement_->getSnapshotNumberByName(name);

    if(not snapshotManagement_->isSnapshotInBackend(num))
    {
        LOG_VERROR("Not restoring snapshot " << name << " because it is not in the backend yet");
        throw fungi::IOException("Not restoring snapshot because it is not in the backend yet");
    }

    sync_(AppendCheckSum::F); // the TLog will be thrown away anyway.

    LOG_VINFO("Removing all tasks from the threadpool for this volume");
    try
    {
        VolManager::get()->backend_thread_pool()->stop(this, 10);

        stopPrefetch_();

        LOG_VINFO("Stopping the readcache");

        // 1) update metadata store
        {
            NSIDMap nsid;
            const SnapshotPersistor& sp(snapshotManagement_->getSnapshotPersistor());
            const yt::UUID cork(sp.getSnapshotCork(name));

            BackendRestartAccumulator acc(nsid,
                                          boost::none,
                                          cork);

            sp.vold(acc,
                    nsidmap_.get(0)->clone(),
                    name);

            metaDataStore_->clear_all_keys();
            metaDataStore_->processCloneTLogs(acc.clone_tlogs(),
                                              nsid,
                                              VolManager::get()->getTLogPath(cfg),
                                              true,
                                              cork);
        }

        // 2) get list of excess SCOs - to be removed only after persisting the new snapshots file
        std::vector<SCO> doomedSCOs;
        OrderedTLogIds doomedTLogs;

        try
        {
            LOG_VINFO("Finding out which TLogs and SCO's to delete");

            doomedTLogs = snapshotManagement_->getTLogsAfterSnapshot(num);

            LOG_VINFO("Deleting " << doomedTLogs.size() << " TLogs");

            const fs::path path(VolManager::get()->getTLogPath(*this));
            std::shared_ptr<TLogReaderInterface> r = CombinedTLogReader::create(path,
                                                                                doomedTLogs,
                                                                                getBackendInterface()->clone());

            r->SCONames(doomedSCOs);
            LOG_VINFO("Deleting " << doomedSCOs.size() << " SCOs");
        }
        catch(std::exception& e)
        {
            LOG_VERROR("Could not get all SCO's that can be deleted from the backend: " << e.what());
        }
        catch(...)
        {
            LOG_VERROR("Could not get all SCO's that can be deleted from the backend: Unknown Exception");
        }

        // 3) persist snapshots file and cleanup TLogs
        snapshotManagement_->eraseSnapshotsAndTLogsAfterSnapshot(num);

        //only from this point on can we do the check
        check_cork_match_();

        LOG_VINFO("Finding out last sco on backend");

        const OrderedTLogIds out(snapshotManagement_->getAllTLogs());

        std::shared_ptr<TLogReaderInterface> itf =
            CombinedTLogReader::create_backward_reader(snapshotManagement_->getTLogsPath(),
                                                       out,
                                                       getBackendInterface()->clone());

        ClusterLocation last_in_backend = itf->nextClusterLocation();
        LOG_VINFO("Resetting datastore to " << last_in_backend);
        dataStore_->restoreSnapshot(last_in_backend.number());

        // 4) reset the FOC
        LOG_VINFO("Resetting the FOC");

        if(foc_config_wrapper_.config())
        {
            try
            {
                failover_->Flush();
                failover_->Clear();
                setVolumeFailOverState(VolumeFailOverState::OK_SYNC);
            }
            // Z42: std::exception?
            catch (std::exception& e)
            {
                setVolumeFailOverState(VolumeFailOverState::DEGRADED);
                LOG_VERROR("Error resetting the failover cache:" << e.what());
            }
        }

        // 6) remove the doomed TLogs
        for (const auto& tlog_id : doomedTLogs)
        {
            backend_task::DeleteTLog* task =
                new backend_task::DeleteTLog(this,
                                             boost::lexical_cast<std::string>(tlog_id));
            VolManager::get()->backend_thread_pool()->addTask(task);
        }


        // 7) remove the doomed SCOs from the datastore + backend
        for (const SCO sconame : doomedSCOs)
        {
            backend_task::DeleteSCO* task = new backend_task::DeleteSCO(this,
                                                              sconame,
                                                              BarrierTask::T);
            VolManager::get()->backend_thread_pool()->addTask(task);
        }

        VolManager::get()->backend_thread_pool()->addTask(new backend_task::Barrier(this));
    }
    CATCH_STD_ALL_EWHAT({
            LOG_VERROR("snapshot restoration failed: " << EWHAT <<
                       " - halting volume");
            halt();
            throw;
        });

    // 8) invalidate any location based cache entries
    reregister_with_cluster_cache_();
}

void
Volume::cloneFromParentSnapshot(const yt::UUID& parent_snap_uuid,
                                const CloneTLogs& clone_tlogs)
{
    // Watch out... at this point we have a TLog and a cork!!
    // We're just going behind it's back to set up everything
    WLOCK();

    const VolumeConfig cfg(get_config());

    const uint64_t max_non_disposable =
        VolManager::get()->get_sco_cache_max_non_disposable_bytes(cfg);

    dataStore_->newVolume(0,
                          max_non_disposable);

    writeConfigToBackend_(cfg);
    writeFailOverCacheConfigToBackend_();
    setVolumeFailOverState(VolumeFailOverState::OK_STANDALONE);

    metaDataStore_->processCloneTLogs(clone_tlogs,
                                      nsidmap_,
                                      VolManager::get()->getTLogPath(cfg),
                                      true,
                                      parent_snap_uuid);

    snapshotManagement_->scheduleWriteSnapshotToBackend();
    check_cork_match_();

    // Set the access probablities
    try
    {
        LOG_VINFO("Getting and rebasing parent access probabilities");
        SCOAccessDataPtr sad;
        {
            SCOAccessDataPersistor parent(nsidmap_.get(1)->clone());
            sad = parent.pull();
        }

        sad->rebase(getNamespace());
        {
            SCOAccessDataPersistor self(nsidmap_.get(0)->clone());
            self.push(*sad,
                      backend_write_condition());
        }
    }
    catch (be::BackendUniqueObjectTagMismatchException&)
    {
        LOG_VWARN("conditional write of SCO access data failed");
        halt();
        throw;
    }

    CATCH_STD_ALL_EWHAT({
            LOG_VINFO("Problem getting and rebasing parent access probabilities: " <<
                      EWHAT << " - proceeding");
        });

    register_with_cluster_cache_(getOwnerTag());
}

void
Volume::destroy(DeleteLocalData delete_local_data,
                RemoveVolumeCompletely remove_volume_completely,
                ForceVolumeDeletion force_volume_deletion)
{
    WLOCK();

#define CATCH_AND_CHECK_FORCE(x, message)       \
    try                                         \
    {                                           \
        x;                                      \
    }                                           \
    catch(std::exception& e)                    \
    {                                           \
        LOG_WARN(message << e.what());          \
        if(F(force_volume_deletion))            \
        {                                       \
            throw;                              \
        }                                       \
    }

    if(not halted_)
    {
        try
         {
             sync_(AppendCheckSum::T);
         }
         catch (std::exception& e)
         {
             LOG_VERROR("was not able to append a sync: " << e.what());
         }
    }

    LOG_VINFO("destroying volume: delete local data " << delete_local_data <<
              ", remove volume completely " << remove_volume_completely <<
              ", force_volume deletion " << force_volume_deletion);

    // SYNC WAS REMOVED HERE

    stopPrefetch_();

    // OVS-827: wait forever, otherwise we might leave tasks that might try call back into
    // a deleted volume.
    const unsigned timeout_secs = std::numeric_limits<unsigned>::max();
    CATCH_AND_CHECK_FORCE(VolManager::get()->backend_thread_pool()->stop(this,
                                                                         timeout_secs),
                          "Exception trying to stop the backend threadpool");

    ASSERT(failover_.get());
    {
        if(T(remove_volume_completely))
        {
            LOG_VINFO("Unregistering volume from ClusterCache");
            deregister_from_cluster_cache_(getOwnerTag());
            CATCH_AND_CHECK_FORCE(failover_->newCache(nullptr),
                                  "Exception trying to unset the failover cache");
        }
        else
        {
            CATCH_AND_CHECK_FORCE(failover_->destroy(SyncFailOverToBackend::F),
                                  "Exception trying to destroy the failover cache");
        }
        failover_.reset();
    }
    ASSERT(snapshotManagement_.get());
    {

        CATCH_AND_CHECK_FORCE(snapshotManagement_->destroy(delete_local_data),
                              "Exception trying to destroy snapshotmanagment");
        snapshotManagement_.reset();
    }

    // why no reset of snapshotManagement
    ASSERT(metaDataStore_.get());
    {
        if(T(delete_local_data))
        {
            metaDataStore_->set_delete_local_artefacts_on_destroy();
        }

        if(T(remove_volume_completely))
        {
            metaDataStore_->set_delete_global_artefacts_on_destroy();
        }
        metaDataStore_.reset();
    }

    ASSERT(dataStore_.get());
    {
        try
        {
            dataStore_->destroy(delete_local_data);
        }
        CATCH_STD_VLOG_IGNORE("Failed to remove DataStore. Manual cleanup required", ERROR);
        dataStore_.reset();
    }

    if (T(delete_local_data))
    {
        CATCH_AND_CHECK_FORCE(fs::remove_all(VolManager::get()->getMetaDataPath(*this)),
                              "Exception trying to delete the metadata directory");
        LOG_VINFO("Unregistering volume from ClusterCache");
        deregister_from_cluster_cache_(getOwnerTag());
    }

#undef CATCH_AND_CHECK_FORCE
}

bool
Volume::isSyncedToBackend() const
{
    return (not halted_) and
        snapshotManagement_->isSyncedToBackend() and
        VolManager::get()->backend_thread_pool()->noTasksPresent(const_cast<Volume*>(this));
}

bool
Volume::isSyncedToBackendUpTo(const SnapshotName& snapshotName) const
{
    SnapshotNum num = snapshotManagement_->getSnapshotNumberByName(snapshotName);
    return snapshotManagement_->isSnapshotInBackend(num);
}

bool
Volume::isSyncedToBackendUpTo(const TLogId& tlog_id) const
{
    return snapshotManagement_->isTLogWrittenToBackend(tlog_id);
}

std::vector<scrubbing::ScrubWork>
Volume::getScrubbingWork(const boost::optional<SnapshotName>& start_snap,
                         const boost::optional<SnapshotName>& end_snap) const
{
    if(T(isVolumeTemplate()))
    {
        LOG_ERROR("Volume " << getName() << " has been templated, scrubbing is not allowed.");
        throw VolumeIsTemplateException("Templated Volume, getting scrub work is forbidden");
    }

    SnapshotWork snap_work;

    snapshotManagement_->getSnapshotScrubbingWork(start_snap,
                                                  end_snap,
                                                  snap_work);

    if (not snap_work.empty())
    {
        metaDataStore_->sync();
    }

    std::vector<scrubbing::ScrubWork> scrub_work;
    scrub_work.reserve(snap_work.size());
    const VolumeConfig cfg(get_config());

    for(const auto& w : snap_work)
    {
        scrub_work.emplace_back(VolManager::get()->getBackendConfig().clone(),
                                cfg.getNS(),
                                cfg.id_,
                                ilogb(cfg.cluster_mult_ * cfg.lba_size_),
                                getSCOMultiplier(),
                                w);
    }

    return scrub_work;
}

void
Volume::cleanupScrubbingOnError_(const be::Namespace& nspace,
                                 const scrubbing::ScrubberResult& scrub_result,
                                 const std::string& res_name)
{
    LOG_VINFO("Cleaning up, don't reapply scrubbing " << res_name);
    for (const auto& tlog : scrub_result.tlogs_out)
    {
        if(snapshotManagement_->tlogReferenced(tlog.id()))
        {
            LOG_VERROR("Scrubbing cleanup wanted to remove " << tlog.id() <<
                       " but it is still referenced in the snapshots file... exiting cleanup");
            return;
        }
    }

    std::vector<std::string> garbage;
    garbage.reserve(scrub_result.new_sconames.size() +
                    scrub_result.tlogs_out.size() +
                    scrub_result.relocs.size() +
                    1);

    for (const SCO sco : scrub_result.new_sconames)
    {
        garbage.emplace_back(sco.str());
    }

    for (const auto& tlog : scrub_result.tlogs_out)
    {
        garbage.emplace_back(tlog.getName());
    }

    for (const std::string& reloc_log : scrub_result.relocs)
    {
        garbage.emplace_back(reloc_log);
    }

    garbage.emplace_back(res_name);

    VolManager::get()->backend_garbage_collector()->queue(be::Garbage(nspace,
                                                                      std::move(garbage)));
}

boost::optional<be::Garbage>
Volume::applyScrubbingWork(const scrubbing::ScrubReply& scrub_reply,
                           const ScrubbingCleanup cleanup,
                           const PrefetchVolumeData prefetch)
{
    const be::Namespace& ns = scrub_reply.ns_;
    const std::string& res_name = scrub_reply.scrub_result_name_;

    LOG_VINFO("namespace: " << ns <<
              ", result name: " << res_name <<
              ", cleanup: " << cleanup <<
              ", prefetch: " << prefetch);

    if(T(isVolumeTemplate()))
    {
        LOG_ERROR("Volume " << getName() <<
                  " has been templated, applying scrubbing is not allowed.");
        throw VolumeIsTemplateException("Templated Volume, applying scrubbing is forbidden");
    }

    checkNotHalted_();

    LOG_VINFO("preliminary work");

    if(res_name.empty())
    {
        LOG_VERROR("Empty scrubbing result passed");
        throw fungi::IOException("No result name passed");
    }

    scrubbing::ScrubberResult scrub_result;
    const SCOCloneID scid = nsidmap_.getCloneID(ns);

    try
    {
        nsidmap_.get(scid)->fillObject<decltype(scrub_result),
                                       boost::archive::text_iarchive>(scrub_result,
                                                                      res_name,
                                                                      InsistOnLatestVersion::T);
    }
    catch (const be::BackendException& e)
    {
        LOG_VERROR("Backend exception retrieving scrub result " << res_name << ": " << e.what());
        throw TransientException("Backend exception retrieving scrub result",
                                 res_name.c_str());
    }
    CATCH_STD_ALL_EWHAT({
        // Manual cleanup required?
            LOG_VERROR("Could not retrieve scrubbing result " << res_name << ": " <<
                       EWHAT);
            VolumeDriverError::report(events::VolumeDriverErrorCode::GetScrubbingResultsFromBackend,
                                      EWHAT,
                                      getName());
            throw;
        });

    std::unique_ptr<RelocationReaderFactory> reloc_reader_factory;

    try
    {
        // Prefetch the relocation logs to prevent a backend glitch from sending the volume into the
        // 'halted' limbo - cf. OVS-3764.
        reloc_reader_factory =
            std::make_unique<RelocationReaderFactory>(scrub_result.relocs,
                                                      FileUtils::temp_path(),
                                                      nsidmap_.get(scid)->clone(),
                                                      CombinedTLogReader::FetchStrategy::Prefetch);
        reloc_reader_factory->prepare_one();
    }
    catch (const be::BackendException& e)
    {
        LOG_VERROR("Backend exception retrieving relocations " << res_name << ": " << e.what());
        throw TransientException("Backend exception retrieving relocations",
                                 res_name.c_str());
    }
    CATCH_STD_ALL_EWHAT({
            LOG_VERROR("Could not get relocation logs from the backend " << res_name << ": " <<
                       EWHAT);
            VolumeDriverError::report(events::VolumeDriverErrorCode::GetScrubbingResultsFromBackend,
                                      EWHAT,
                                      getName());
            throw;
        });

    MaybeScrubId scrub_id;

    try
    {
        if(scid == 0)
        {
            LOG_VINFO("ApplyScrub: replacing tlogs for snapshot " <<
                      scrub_result.snapshot_name);

            VERIFY(getNamespace() == ns);
            if(not scrub_result.tlogs_out.empty())
            {
                const SnapshotNum n =
                    snapshotManagement_->getSnapshotNumberByName(scrub_result.snapshot_name);
                // Y42 I think this is a point where local restart will fail??
                scrub_id =
                    snapshotManagement_->replaceTLogsWithScrubbedOnes(scrub_result.tlog_names_in,
                                                                      scrub_result.tlogs_out,
                                                                      n);
                auto b(new backend_task::Barrier(this));
                VolManager::get()->scheduleTask(b);
            }
            LOG_VINFO("Finished replacing tlogs");
        }

        if (scrub_id == boost::none)
        {
            TODO("AR: revisit: if a parent was scrubbed we need to be sure that its snapshots.xml made it to the backend before applying relocations");
            VERIFY(scid != SCOCloneID(0));
            LOG_VINFO("Parent was scrubbed? We need a new scrub ID nevertheless.");
            scrub_id = snapshotManagement_->new_scrub_id();
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_VERROR("Failed to apply scrub results of snapshot " <<
                       scrub_result.snapshot_name <<
                       " to SnapshotManagement: " << EWHAT);

            switch (cleanup)
            {
            case ScrubbingCleanup::OnError:
            case ScrubbingCleanup::Always:
                cleanupScrubbingOnError_(ns,
                                         scrub_result,
                                         res_name);
                break;
            default:
                break;
            }

            VolumeDriverError::report(events::VolumeDriverErrorCode::ApplyScrubbingToSnapshotMamager,
                                      EWHAT,
                                      getName());
            throw;
        });

    LOG_VINFO("New scrub ID: " << scrub_id);

    VERIFY(scrub_id != boost::none);

    try
    {
        LOG_VINFO("applying " << scrub_result.relocs.size() << " relocation tlogs");
        const uint64_t relocNum = metaDataStore_->applyRelocs(*reloc_reader_factory,
                                                              scid,
                                                              *scrub_id);

        if(relocNum == scrub_result.relocNum)
        {
            LOG_VINFO("Number of relocations applied to the MetaDataStore: " << relocNum);
        }
        else
        {
            LOG_VERROR("Number of relocations applied to the MetaDataStore: " <<
                       relocNum << " but " << scrub_result.relocNum <<
                       " were reported by the Scrubber");
            throw fungi::IOException("Incorrect number of relocations replayed on metadatastore");
        }
    }
    CATCH_STD_ALL_EWHAT({
            VolumeDriverError::report(events::VolumeDriverErrorCode::ApplyScrubbingRelocs,
                                      EWHAT,
                                      getName());
            halt();
            throw;
        });

    LOG_VINFO("Finished applying relocations");
    LOG_VINFO("Removing deleted containers from SCO cache");

    // Y42 This depends on the fact that scos are not shared across namespaces
    for(std::vector<SCO>::iterator it = scrub_result.sconames_to_be_deleted.begin();
        it != scrub_result.sconames_to_be_deleted.end();
        ++it)
    {
        VERIFY(it->cloneID() == 0);
        it->cloneID(scid);
        LOG_VINFO("Removing SCO " << *it << " from datastore");
        dataStore_->removeSCO(*it);
    }

    LOG_VINFO("Finished removing deleted containers from SCO cache");

    if(T(prefetch))
    {
        LOG_VINFO("Prefetching");
        // Ideally we want to remove the deleted scos from the datastore so they can
        // make place for the pretching stuff.
        for(std::vector<std::pair<SCO, float> >::const_iterator it = scrub_result.prefetch.begin();
            it != scrub_result.prefetch.end();
            ++it)
        {
            float vra_sum = VolManager::get()->readActivity();
            if (vra_sum == 0)
            {
                vra_sum = 1;
            }

            float vra_factor = readActivity() / vra_sum;

            SCO sco = it->first;
            getPrefetchData().addSCO(sco,
                                     vra_factor * it->second);
        }
        LOG_VINFO("Finished Prefetching");
    }
    else
    {
        LOG_VINFO("Not Prefetching");
    }

    boost::optional<be::Garbage> maybe_garbage;

    if(scid == 0)
    {
        LOG_VINFO("Determining garbage");

        std::vector<std::string> garbage;
        garbage.reserve(scrub_result.tlog_names_in.size() +
                        scrub_result.relocs.size() +
                        scrub_result.sconames_to_be_deleted.size() +
                        1);

        for (const auto& tlog_id : scrub_result.tlog_names_in)
        {
            garbage.emplace_back(boost::lexical_cast<std::string>(tlog_id));
        }

        for (const auto& reloc_log : scrub_result.relocs)
        {
            garbage.emplace_back(reloc_log);
        }

        for (const SCO sco : scrub_result.sconames_to_be_deleted)
        {
            garbage.emplace_back(sco.str());
        }

        garbage.emplace_back(res_name);

        maybe_garbage = be::Garbage(ns,
                                    std::move(garbage));

        LOG_VINFO("finished determining garbage");
    }

    return maybe_garbage;
}

SnapshotName
Volume::getParentSnapName() const
{
    LOCK_CONFIG();
    return config_.parent_snapshot_;
}

void
Volume::removeUpToFromFailOverCache(const SCO sconame)
{
    // Y42 can the failover be shot down while we are flushing??
    // I think it can and we need to protect agains it.
    if(failover_->backup())
    {
        LOG_VINFO("removing up to sconame " << sconame <<
                  " from the FOC");
        failover_->Flush();
        failover_->removeUpTo(sconame);
        LOG_VINFO("done removing up to sconame " << sconame <<
                  " from the FOC");
    }
}

void
Volume::replayClusterFromFailOverCache_(ClusterAddress ca,
                                        const ClusterLocation& loc,
                                        const uint8_t* buf)
{
    ASSERT_WRITES_SERIALIZED();
    ASSERT_WLOCKED();

    LOG_VTRACE("ca " << ca << ", loc " << loc << ", buf " << &buf);
    checkNotHalted_();

    uint32_t throttle = 0;
    MaybeCheckSum forced_rollover;

    if (loc.offset() == 0)
    {
        LOG_VTRACE("forced SCO rollover");
        forced_rollover = dataStore_->finalizeCurrentSCO();
    }

    // This is an interesting location - we could end up creating SCOs larger
    // than what is currently configured, i.e.
    // dataStore_->getRemainingSCOCapacity() could return values < 0.
    // So no sanity check here!
    dataStore_->writeClusterToLocation(buf,
                                       loc,
                                       throttle);

    // if the SCO is filled up at this stage it will be either rolled over on
    // the next cluster replay or - if there is no next cluster - as part of
    // the subsequent FOC replay steps (cf. replayFOC_).
    if(forced_rollover)
    {
        snapshotManagement_->addSCOCRC(*forced_rollover);
    }

    ClusterLocationAndHash loc_and_hash(loc,
                                        buf,
                                        getClusterSize());
    writeClusterMetaData_(ca,
                          loc_and_hash);

    if(throttle > 0)
    {
        throttle_(throttle);
    }
}

uint64_t
Volume::VolumeDataStoreWriteUsed() const
{
    return dataStore_->getWriteUsed();
}

uint64_t
Volume::VolumeDataStoreReadUsed() const
{
    return dataStore_->getReadUsed();
}

uint64_t
Volume::getTLogUsed() const
{
    const std::vector<fs::path>
        paths(snapshotManagement_->tlogPathPrepender(snapshotManagement_->getAllTLogs()));
    uint64_t res = 0;
    struct stat st;

    for (const auto& path : paths)
    {
        if (::stat(path.string().c_str(), &st) == 0)
        {
            if(S_ISREG(st.st_mode))
            {
                res += st.st_size;
            }
        }
    }

    return res;
}


void
Volume::check_and_fix_failovercache()
{
    if(failoverstate_ == VolumeFailOverState::DEGRADED)
    {
        LOG_VPERIODIC("Volume State is " << failoverstate_ << ", will try to fix");
        if(not foc_config_wrapper_.config())
        {
            LOG_VPERIODIC("Trying to unset the failover cache");
            try
            {
                setFailOverCacheConfig(boost::none);
            }
            catch(std::exception& e)
            {
                LOG_VPERIODIC("Exception while trying to unset the failover cache: " << e.what());
                return;
            }
            catch(...)
            {
                LOG_VPERIODIC("Unknown exception while trying to unset the failover cache. ");
                return;
            }

        }
        else
        {
            ASSERT(foc_config_wrapper_.config());

            LOG_VPERIODIC("Trying to set the failover cache to " <<
                          *foc_config_wrapper_.config());
            try
            {
                setFailOverCacheConfig(foc_config_wrapper_.config());
            }
            catch(std::exception& e)
            {
                LOG_VPERIODIC("Exception while trying to set the failover cache: " << e.what());
                return;
            }
            catch(...)
            {
                LOG_VPERIODIC("Unknown exception while trying to set the failover cache. ");
                return;
            }
        }

    }
    else
    {
        LOG_VPERIODIC("Volume state is " << failoverstate_ << " seems ok.");
    }
}

TLogId
Volume::scheduleBackendSync()
{
    WLOCK();

    checkNotHalted_();
    return scheduleBackendSync_();
}

TLogId
Volume::scheduleBackendSync_()
{
    // TODO: halt the volume in case of non-recoverable errors
    sync_(AppendCheckSum::F);

    const auto maybe_sco_crc(dataStore_->finalizeCurrentSCO());
    return snapshotManagement_->scheduleBackendSync(maybe_sco_crc);
}

void
Volume::setFailOverCacheConfig(const boost::optional<FailOverCacheConfig>& config)
{
    WLOCK(); // Z42: make more fine grained?

    if (config)
    {
        setFailOverCacheConfig_(*config);
    }
    else
    {
        setNoFailOverCache_();
    }
}

boost::optional<FailOverCacheConfig>
getFailOverCacheConfig()
{
    return boost::none; // TODO AT for now
}

void
Volume::init_failover_cache_()
{
    failover_->setBusyLoopDuration(bc::microseconds(VolManager::get()->dtl_busy_loop_usecs.value()));

    failover_->initialize([&]() noexcept
                          {
                              LOG_VINFO("setting DTL degraded");
                              setVolumeFailOverState(VolumeFailOverState::DEGRADED);
                          });
}

void
Volume::setFailOverCacheMode_(const FailOverCacheMode mode)
{
    if (mode != failover_->mode())
    {
        failover_->destroy(SyncFailOverToBackend::T);
        failover_ = FailOverCacheClientInterface::create(mode,
                                                         LBASize(getLBASize()),
                                                         getClusterMultiplier(),
                                                         VolManager::get()->dtl_queue_depth.value(),
                                                         VolManager::get()->dtl_write_trigger.value());
        init_failover_cache_();
    }
}

void
Volume::setFailOverCacheConfig_(const FailOverCacheConfig& config)
{
    // This seems only to be called externally.
    checkNotHalted_();
    setVolumeFailOverState(VolumeFailOverState::DEGRADED);
    foc_config_wrapper_.set(config);

    last_tlog_not_on_failover_ = boost::none;
    failover_->newCache(nullptr);

    writeFailOverCacheConfigToBackend_();

    LOG_VINFO("Setting the failover cache to " << config);
    setFailOverCacheMode_(config.mode);
    failover_->newCache(std::make_unique<FailOverCacheProxy>(config,
                                                             getNamespace(),
                                                             LBASize(getLBASize()),
                                                             getClusterMultiplier(),
                                                             failover_->getDefaultRequestTimeout()));
    failover_->Clear();

    MaybeCheckSum cs = dataStore_->finalizeCurrentSCO();

    LOG_VINFO("finding out volume state");
    {
        OrderedTLogIds out;
        fungi::ScopedSpinLock l(volumeStateSpinLock_);

        snapshotManagement_->getTLogsNotWrittenToBackend(out);
        const int size = out.size();

        VERIFY(out.size() >= 1);
        const TLogId curTLog(snapshotManagement_->getCurrentTLogId());
        VERIFY(out.back() == curTLog);

        if(snapshotManagement_->currentTLogHasData())
        {
            last_tlog_not_on_failover_ = curTLog;
            setVolumeFailOverState(VolumeFailOverState::KETCHUP);
        }
        else if(size > 1)
        {
            last_tlog_not_on_failover_ = out[size - 2];
            setVolumeFailOverState(VolumeFailOverState::KETCHUP);
        }
        else
        {
            //Y42 ONLY CORRECT IF THE CURRENT TLOG HOLDS NO DATA!!
            setVolumeFailOverState(VolumeFailOverState::OK_SYNC);
        }
    }

    // halt the volume in case of non-recoverable errors
    snapshotManagement_->scheduleBackendSync(cs);
}

void
Volume::setNoFailOverCache_()
{
    ASSERT_WLOCKED();
    checkNotHalted_();

    setVolumeFailOverState(VolumeFailOverState::DEGRADED);
    last_tlog_not_on_failover_ = boost::none;
    foc_config_wrapper_.set(boost::none);

    // these can throw
    failover_->newCache(nullptr);
    writeFailOverCacheConfigToBackend_();

    setVolumeFailOverState(VolumeFailOverState::OK_STANDALONE);
}

void
Volume::checkState(const TLogId& tlog_id)
{
    fungi::ScopedSpinLock l(volumeStateSpinLock_);

    if(last_tlog_not_on_failover_ and
       *last_tlog_not_on_failover_ == tlog_id)
    {
        setVolumeFailOverState(VolumeFailOverState::OK_SYNC);
    }
}

void
Volume::setVolumeFailOverState(VolumeFailOverState s)
{
    std::swap(failoverstate_, s);

    if (s != failoverstate_)
    {
        LOG_VINFO("transitioned from " << s << " -> " << failoverstate_);

        VolumeDriverError::report(getName(),
                                  s,
                                  failoverstate_);
    }
}

void
Volume::tlogWrittenToBackendCallback(const TLogId& tid,
                                     const SCO sconame)
{
    snapshotManagement_->tlogWrittenToBackendCallback(tid,
                                                      sconame);
}

uint64_t
Volume::getSnapshotSCOCount(const SnapshotName& snapshotName)
{
    OrderedTLogIds tlog_ids;
    if(snapshotName.empty())
    {
        tlog_ids = snapshotManagement_->getCurrentTLogs();
    }
    else
    {
        const SnapshotNum snap = snapshotManagement_->getSnapshotNumberByName(snapshotName);
        snapshotManagement_->getTLogsInSnapshot(snap,
                                                tlog_ids);
    }

    std::shared_ptr<TLogReaderInterface>
        treader(CombinedTLogReader::create(snapshotManagement_->getTLogsPath(),
                                           tlog_ids,
                                           getBackendInterface()->clone()));

    std::vector<SCO> lst;
    treader->SCONames(lst);
    return lst.size();
}

uint64_t
Volume::getCacheHits() const
{
    return dataStore_->getCacheHits();
}

uint64_t
Volume::getCacheMisses() const
{
    return dataStore_->getCacheMisses();
}

uint64_t
Volume::getNonSequentialReads() const
{
    // Z42: reinstate once we've got a good idea what to measure :)
    return 0;
}

uint64_t
Volume::getClusterCacheHits() const
{
    return readCacheHits_;
}

uint64_t
Volume::getClusterCacheMisses() const
{
    return readCacheMisses_;
}

void
Volume::setFOCTimeout(const boost::chrono::seconds timeout)
{
    checkNotHalted_();
    failover_->setRequestTimeout(timeout);
}

void
Volume::sync()
{
    tracepoint(openvstorage_volumedriver,
               volume_sync_start,
               config_.id_.str().c_str());

    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         tracepoint(openvstorage_volumedriver,
                                                    volume_sync_end,
                                                    config_.id_.str().c_str(),
                                                    std::uncaught_exception());
                                     }));

    if (VolManager::get()->volume_nullio.value())
    {
        return;
    }

    yt::SteadyTimer t;

    uint64_t number_of_syncs_to_ignore, maximum_time_to_ignore_syncs_in_seconds;
    std::tie(number_of_syncs_to_ignore, maximum_time_to_ignore_syncs_in_seconds) = getSyncSettings();

    WLOCK();

    ++total_number_of_syncs_;

    if((++number_of_syncs_ > number_of_syncs_to_ignore) or
       (sync_wall_timer_.elapsed_in_seconds() > maximum_time_to_ignore_syncs_in_seconds))
    {
        number_of_syncs_ = 0;
        sync_wall_timer_.restart();
        sync_(AppendCheckSum::F);
    }

    const auto duration_us(bc::duration_cast<bc::microseconds>(t.elapsed()));
    performance_counters().sync_request_usecs.count(duration_us.count());
}

void
Volume::sync_(AppendCheckSum append_chksum)
{
    ASSERT_WLOCKED();

    LOG_VTRACE("syncing volume");

    checkNotHalted_();
    const auto maybe_sco_crc(dataStore_->sync());
    // No more syncing here by order of BDV... we do this in the uncork now
    //    metaDataStore_->sync();

    snapshotManagement_->sync(append_chksum == AppendCheckSum::T ?
                              maybe_sco_crc :
                              boost::none);

    failover_->Flush();
}

bool
Volume::checkSCONamesConsistency_(const std::vector<SCO>& names)
{
    //must and does work with empty vector
    //checks increasing sconumbers and single version per sconumber

    SCO lastName;
    for (unsigned i = 0; i < names.size(); i++)
    {
        if (names[i].cloneID() != SCOCloneID(0))
        {
            LOG_ERROR("cloneID in SCOName not 0 " << names[i]);
            return false;
        }

        if (not (names[i] == lastName))
        {
            if (not (names[i].number() > lastName.number()))
            {
                LOG_ERROR("SCO numbers are not increasing within tlog from "
                          << lastName << " to " << names[i]);
                return false;
            }
            lastName = names[i];
        }
    }
    return true;
}

bool
Volume::checkTLogsConsistency_(CloneTLogs& ctl) const
{
    const fs::path tlog_temp_location(VolManager::get()->getTLogPath(*this) / "tmp");
    fs::create_directories(tlog_temp_location);

    for(unsigned i = 0; i < ctl.size(); ++i)
    {
        SCOCloneID cloneid = ctl[i].first;
        BackendInterfacePtr bi = getBackendInterface(cloneid)->clone();
        OrderedTLogIds& tlogs = ctl[i].second;
        SCONumber lastSCONumber = 0;

        for(unsigned k = 0; k < tlogs.size(); k++)
        {
            std::vector<SCO> scoNames;
            TLogReader(tlog_temp_location,
                       boost::lexical_cast<std::string>(tlogs[k]),
                       bi->clone()).SCONames(scoNames);

            if (not (scoNames.empty() or
                     (scoNames.front().number() > lastSCONumber)))
            {
                LOG_VERROR("SCO number at beginning of TLog "
                           << tlogs[k] <<
                           "has not increased compared to last SCO from previous TLog");
                return false;
            }

            if (not checkSCONamesConsistency_(scoNames))
            {
                LOG_VERROR("SCO names are not consistent");

                return false;
            }

            for (unsigned i = 0; i < scoNames.size(); i++)
            {
                std::string name =scoNames[i].str();
                if (not bi->objectExists(name))
                {
                    LOG_VERROR("SCO " << name << " in tlog " << tlogs[k] <<
                               " does not exist on backend");

                    return false;
                }
            }

            if (not scoNames.empty())
            {
                lastSCONumber = scoNames.back().number();
            }
        }
    }
    return true;
}

namespace
{

struct CheckConsistencyAcc
{
    CheckConsistencyAcc(CloneTLogs& result)
        : result_(result)
    { }
    DECLARE_LOGGER("CheckConsistencyAcc")
    void
    operator()(const SnapshotPersistor* s,
               BackendInterfacePtr&,
               const SnapshotName& snap,
               SCOCloneID clone_id)
    {
        OrderedTLogIds tlogs;
        if(clone_id == SCOCloneID(0))
        {
            VERIFY(snap.empty());
            s->getTLogsWrittenToBackend(tlogs);
        }
        else
        {
            VERIFY(not snap.empty());
            s->getTLogsTillSnapshot(snap, tlogs);
        }
        result_.push_back(std::make_pair(clone_id, std::move(tlogs)));
    }

    CloneTLogs& result_;
};

}

bool
Volume::checkConsistency()
{
    WLOCK();

    if(not isSyncedToBackend())
    {
        LOG_VERROR("Volume is not synced to backend, can't check consistency");
        throw fungi::IOException("Volume not synced to backend");
    }
    return true;
}

uint64_t
Volume::getSnapshotBackendSize(const SnapshotName& snapName)
{
    return snapshotManagement_->getSnapshotBackendSize(snapName);
}

uint64_t
Volume::getCurrentBackendSize() const
{
    return snapshotManagement_->getCurrentBackendSize();
}

uint64_t
Volume::getTotalBackendSize() const
{
    return snapshotManagement_->getTotalBackendSize();
}

void
Volume::SCOWrittenToBackendCallback(uint64_t file_size,
                                    bc::microseconds write_time)
{
    performance_counters().backend_write_request_size.count(file_size);
    performance_counters().backend_write_request_usecs.count(write_time.count());
}

void
Volume::replayFOC_(FailOverCacheProxy& foc)
{
    ASSERT_WRITES_SERIALIZED();
    ASSERT_WLOCKED();

    auto fun([&](ClusterLocation loc,
                 uint64_t lba,
                 const byte* buf,
                 size_t size)
             {
                 uint32_t counter = 0;
                 LOG_VTRACE("Replaying " << loc << "lba " << lba);

                 VERIFY(size == static_cast<size_t>(clusterSize_));

                 validateIOAlignment(lba, size);

                 while (true)
                 {
                     try
                     {
                         replayClusterFromFailOverCache_(addr2CA(LBA2Addr(lba)),
                                                         loc,
                                                         buf);
                         return;
                     }
                     catch (TransientException& e)
                     {
                         LOG_VINFO("TransientException");
                         if (++counter == 256)
                         {
                             throw;
                         }
                         boost::this_thread::sleep_for(bc::seconds(1));
                     }
                 }
             });

    try
    {
        foc.getEntries(std::move(fun));

        MaybeCheckSum cs = dataStore_->finalizeCurrentSCO();

        if (snapshotManagement_->currentTLogHasData())
        {
            snapshotManagement_->scheduleBackendSync(cs);
        }
    }
    CATCH_STD_ALL_EWHAT({
            LOG_VERROR("Problem with failover cache " << EWHAT);
            throw fungi::IOException((std::string("Problem with failover cache ") + EWHAT));
        });
}

double
Volume::readActivity() const
{
    return read_activity_;
}

void
Volume::updateReadActivity(time_t diff)
{
    LOG_VPERIODIC("Updating read activity");
    double zeta = std::min(1.0, VolManager::gamma * diff);
    read_activity_ = (1.0 - zeta) * read_activity_ + zeta * (readcounter_ / (double)diff);
    readcounter_ = 0;
}

void
Volume::stopPrefetch_()
{
    LOG_VINFO("Stopping the prefetching");

    prefetch_data_.stop();
    // Y42 not sure do we want a timed join here?
    if(prefetch_thread_.get())
    {
        prefetch_thread_->join();
        prefetch_thread_.reset();
    }
}

void
Volume::startPrefetch_(const SCOAccessData::VectorType& sadv,
                       SCONumber scoNum)
{
    //don't prefetch sco's that have less than 1 out of 10000 chance of being read for the volume
    static const float SAPTRES = 0.0001;

    float vra_sum = VolManager::get()->readActivity();
    if (vra_sum == 0)
    {
        vra_sum = 1;
    }

    float vra_factor = readActivity() / vra_sum;

    int numscos = 0;
    for (SCOAccessData::VectorType::const_iterator it = sadv.begin();
         it != sadv.end();
         ++it)
    {
        SCO scoName = it->first;
        float sco_sap = it->second;

        if (scoName.cloneID() == 0 &&
            scoNum < scoName.number())
        {
            LOG_VINFO(getNamespace() << "/" << scoName
                      << " newer than last known SCO number "
                      << scoNum
                      << " - not prefetching it");
        }
        else if (sco_sap < SAPTRES)
        {
            LOG_VDEBUG(getNamespace() << "/" << scoName
                       << " has too low access probability for being prefetched ("
                       << sco_sap << "). ");
        }
        else
        {
            LOG_VDEBUG(getNamespace() << "/" << scoName
                       << " with access probability "
                       << sco_sap << " is being prefetched.");
            getPrefetchData().addSCO(scoName,
                                     vra_factor * sco_sap);
            numscos++;
        }
    }
    LOG_VINFO("started prefetching of " << numscos << " SCOs");
}

void
Volume::normalizeSAPs_(SCOAccessData::VectorType& sadv)
{
    float sum = 0;
    for (SCOAccessData::VectorType::const_iterator it = sadv.begin();
         it != sadv.end();
         ++it)
    {
        sum += it->second;
    }

    if (sum == 0)
    {
        sum = 1;
    }

    for (SCOAccessData::VectorType::iterator it = sadv.begin();
         it != sadv.end();
         ++it)
    {
        it->second /= sum;
    }
}

void
Volume::writeConfigToBackend_(const VolumeConfig& cfg)
{
    checkNotHalted_();

    try
    {
        getBackendInterface()->writeObject(cfg,
                                           VolumeConfig::config_backend_name,
                                           OverwriteObject::T,
                                           backend_write_condition());
    }
    catch (be::BackendUniqueObjectTagMismatchException&)
    {
        LOG_VWARN("conditional write of " << VolumeConfig::config_backend_name << " failed");
        halt();
        throw;
    }
}

void
Volume::writeFailOverCacheConfigToBackend_()
{
    checkNotHalted_();

    try
    {
        getBackendInterface()->writeObject(foc_config_wrapper_,
                                           FailOverCacheConfigWrapper::config_backend_name,
                                           OverwriteObject::T,
                                           backend_write_condition());
    }
    catch (be::BackendUniqueObjectTagMismatchException&)
    {
        LOG_VWARN("conditional write of " << VolumeConfig::config_backend_name << " failed");
        halt();
        throw;
    }
}

void
Volume::startPrefetch(SCONumber last_sco_number)
{
    try
    {
        SCOAccessDataPersistor
            sadp(getBackendInterface()->cloneWithNewNamespace(getNamespace()));
        SCOAccessDataPtr sad(sadp.pull());

        read_activity_ = sad->read_activity();

        // Z42: avoid the copy and modify the SCOAccessData internal vector instead?
        SCOAccessData::VectorType sadv(sad->getVector());

        normalizeSAPs_(sadv);

        if(last_sco_number == 0)
        {
            last_sco_number = dataStore_->getCurrentSCONumber();
        }

        startPrefetch_(sadv,
                       last_sco_number);

    }
    catch(std::exception& e)
    {
        LOG_VINFO("Did not start prefetching on volume: " <<
                  e.what());
    }
}

void
Volume::setSCOCacheLimitMax(uint64_t max_non_disposable,
                            const backend::Namespace& nsname)
{
    VolManager::get()->getSCOCache()->setNamespaceLimitMax(nsname,
                                                           max_non_disposable);
}

fs::path
Volume::getTempTLogPath() const
{
    // Y42 maybe just create this once in the constructor of the volume
    fs::path p = snapshotManagement_->getTLogsPath();
    p /= "tmp";
    fs::create_directories(p);
    return p;
}

fs::path
Volume::getCurrentTLogPath_() const
{
    return snapshotManagement_->getTLogsPath();
}

const SnapshotManagement&
Volume::getSnapshotManagement() const
{
    return *snapshotManagement_;
}

void
Volume::halt()
{
    VolumeDriverError::report(events::VolumeDriverErrorCode::VolumeHalted,
                              "halting volume",
                              getName());

    LOG_VFATAL("Setting volume to 'halted'");
    halted_ = true;
    try
    {
        VolManager::get()->backend_thread_pool()->stop(this, 10);
    }
    catch (std::exception& e)
    {
        LOG_VERROR("failed to clear volume's queue: " << e.what() <<
                   " - ignoring");
    }
    catch (...)
    {
        LOG_VERROR("failed to clear volume's queue: unknown error - ignoring");
    }

    try
    {
        VolManager::get()->backend_thread_pool()->halt(this, 10);
    }
    catch (...)
    {
        LOG_VERROR("failed to stop volume's queue: unknown error - ignoring");
    }

    dumpDebugData();
}

bool
Volume::is_halted() const
{
    return halted_;
}

fs::path
Volume::saveSnapshotToTempFile()
{
    return snapshotManagement_->saveSnapshotToTempFile();
}

void
Volume::checkNotHalted_() const
{
    if (halted_)
    {
        throw fungi::IOException("Volume is halted due to previous errors",
                                 getName().c_str());
    }
}

void
Volume::checkNotReadOnly_() const
{
    if (readOnlyMode)
    {
        throw fungi::IOException("Temporarily in readonly mode", getName().c_str());
    }
}

ClusterCacheVolumeInfo
Volume::getClusterCacheVolumeInfo() const
{
    return ClusterCacheVolumeInfo(readCacheHits_, readCacheMisses_);
}

void
Volume::throttle_(unsigned throttle_usecs) const
{
    // not needed but desired, otherwise the throttling will be pointless
    ASSERT_WRITES_SERIALIZED();

    if (throttle_usecs != 0)
    {
        tracepoint(openvstorage_volumedriver,
                   volume_throttle_start,
                   config_.id_.str().c_str(),
                   throttle_usecs);

        auto on_exit(yt::make_scope_exit([&]
                                         {
                                             tracepoint(openvstorage_volumedriver,
                                                        volume_throttle_end,
                                                        config_.id_.str().c_str(),
                                                        throttle_usecs);
                                         }));

        LOG_VDEBUG("throttling writes for " << throttle_usecs <<
                   " us");
        usleep(throttle_usecs);
    }
}

bool
Volume::isCacheOnWrite() const
{
    return
        effective_cluster_cache_behaviour() == ClusterCacheBehaviour::CacheOnWrite;
}

bool
Volume::isCacheOnRead() const
{
    return
        effective_cluster_cache_behaviour() == ClusterCacheBehaviour::CacheOnRead;
}

ClusterCacheHandle
Volume::getClusterCacheHandle() const
{
    // We want to be punished real hard if we forgot to register.
    return *cluster_cache_handle_;
}

ClusterCacheBehaviour
Volume::effective_cluster_cache_behaviour() const
{
    LOCK_CONFIG();

    const boost::optional<ClusterCacheBehaviour>& b =
        config_.cluster_cache_behaviour_;

    if (not b)
    {
        return VolManager::get()->get_cluster_cache_default_behaviour();
    }
    else
    {
        return *b;
    }
}

ClusterCacheMode
Volume::effective_cluster_cache_mode() const
{
    LOCK_CONFIG();

    const boost::optional<ClusterCacheMode>& m = config_.cluster_cache_mode_;

    if (not m)
    {
        return VolManager::get()->get_cluster_cache_default_mode();
    }
    else
    {
        return *m;
    }
}

void
Volume::cork(const youtils::UUID& cork)
{
    metaDataStore_->cork(cork);
}

void
Volume::unCorkAndTrySync(const youtils::UUID& cork)
{
    metaDataStore_->unCork(cork);
    try
    {
        metaDataStore_->sync();
    }
    CATCH_STD_ALL_VLOG_IGNORE("problem syncing metadatastore after uncork")
}

void
Volume::resize(uint64_t clusters)
{
    WLOCK();

    uint64_t lba_count = getLBACount();
    ClusterMultiplier cmult = getClusterMultiplier();

    VERIFY((lba_count % cmult) == 0);

    const uint64_t maxcl = VolManager::get()->real_max_volume_size() / clusterSize_;
    const uint64_t ncl = lba_count / cmult;
    LOG_VINFO("Resizing volume from " << ncl << " to " << clusters << " clusters");

    // Shrinking a volume is quite complex as we need to get rid of stale data
    // (remove data (not only current data but also old data once a snapshot is
    // removed!, shrink the mdstore, ... - let's not go there for the moment.
    if (ncl > clusters)
    {
        LOG_VERROR("Cannot shrink volume from " << ncl << " to " << clusters <<
                   " clusters");
        throw fungi::IOException("Cannot shrink volume",
                                 getName().c_str());
    }
    else if (clusters > maxcl)
    {
        LOG_VERROR("Cannot grow volume to " << clusters <<
                   " clusters as that exceeds the limit of " << maxcl << " clusters");
        throw fungi::IOException("Cannot grow volume beyond limit",
                                 getName().c_str());
    }
    else if (ncl < clusters)
    {
        update_config_([&](VolumeConfig& cfg)
                       {
                           cfg.lba_count() = clusters * cmult;
                       });

        LOG_VINFO("volume grew from " << ncl << " to " << clusters << " clusters");
    }
}

void
Volume::updateMetaDataBackendConfig(const MetaDataBackendConfig& mcfg)
{
    SERIALIZE_WRITES();
    WLOCK();

    metaDataStore_->updateBackendConfig(mcfg);

    update_config_([&](VolumeConfig& vcfg)
                   {
                       vcfg.metadata_backend_config_ =
                           metaDataStore_->getBackendConfig();
                   });
}

void
Volume::set_cluster_cache_behaviour(const boost::optional<ClusterCacheBehaviour>& behaviour)
{
    LOG_VINFO("Setting the cluster cache behaviour to " << behaviour);

    // too heavy locking?
    SERIALIZE_WRITES();
    WLOCK();

    const bool use_cache_old =
        effective_cluster_cache_behaviour() != ClusterCacheBehaviour::NoCache;

    update_config_([&](VolumeConfig& cfg)
                   {
                       cfg.cluster_cache_behaviour_ = behaviour;
                   });

    const bool use_cache_new =
        effective_cluster_cache_behaviour() != ClusterCacheBehaviour::NoCache;

    if (use_cache_old != use_cache_new)
    {
        reregister_with_cluster_cache_();
    }
}

void
Volume::set_cluster_cache_mode(const boost::optional<ClusterCacheMode>& mode)
{
    LOG_VINFO("Setting the cluster cache mode to " << mode);

    const ClusterCacheMode old = effective_cluster_cache_mode();
    const ClusterCacheMode new_ = mode ?
        *mode :
        VolManager::get()->get_cluster_cache_default_mode();

    if (old == ClusterCacheMode::LocationBased and
        new_ == ClusterCacheMode::ContentBased)
    {
        LOG_VERROR("Changing the cluster cache mode from " << old <<
                   " -> " << new_ << " is not supported");
        throw fungi::IOException("Changing the cluster cache mode from LocationBased to ContentBased is not supported");
    }

    // too heavy locking?
    SERIALIZE_WRITES();
    WLOCK();

    update_config_([&](VolumeConfig& cfg)
                   {
                       cfg.cluster_cache_mode_ = mode;
                   });

    if (old != new_)
    {
        reregister_with_cluster_cache_();
    }
}

void
Volume::set_cluster_cache_limit(const boost::optional<ClusterCount>& limit)
{
    LOG_VINFO("Setting the cluster cache limit to " << limit);
    if (limit and *limit == ClusterCount(0))
    {
        LOG_VERROR("cluster cache limit " << *limit << " is not supported");
        throw fungi::IOException("Invalid cluster cache limit");
    }

    SERIALIZE_WRITES();
    WLOCK();

    update_config_([&](VolumeConfig& cfg)
                   {
                       cfg.cluster_cache_limit_ = limit;
                   });

    update_cluster_cache_limit_();
}

void
Volume::update_cluster_cache_limit_()
{
    const boost::optional<ClusterCount> l(get_cluster_cache_limit());

    TODO("AR: use ClusterCount within the ClusterCache as well!?");
    boost::optional<uint64_t> m;
    if (l)
    {
        m = l->t;
    }

    switch (effective_cluster_cache_mode())
    {
    case ClusterCacheMode::LocationBased:
        {
            LOG_VINFO("Updating location based cluster cache limit to " << m);
            VolManager::get()->getClusterCache().set_max_entries(getClusterCacheHandle(),
                                                                 m);
            break;
        }
    case ClusterCacheMode::ContentBased:
        {
            LOG_VINFO("Not updating content based cluster cache limit to " << m);
            break;
        }
    }
}

void
Volume::register_with_cluster_cache_(OwnerTag otag)
{
    cluster_cache_handle_ =
        VolManager::get()->getClusterCache().registerVolume(otag,
                                                            effective_cluster_cache_mode());

    LOG_VINFO("registered with cluster cache, owner tag " << otag <<
              ", cluster cache handle " << cluster_cache_handle_);

    update_cluster_cache_limit_();
}

void
Volume::deregister_from_cluster_cache_(OwnerTag otag)
{
    VolManager::get()->getClusterCache().deregisterVolume(otag);
    cluster_cache_handle_ = boost::none;
}

void
Volume::reregister_with_cluster_cache_(OwnerTag old,
                                       OwnerTag new_)
{
    deregister_from_cluster_cache_(old);
    LOG_VINFO("unregistered from cluster cache, owner tag " << old);
    register_with_cluster_cache_(new_);
}

void
Volume::reregister_with_cluster_cache_()
{
    const OwnerTag otag(getOwnerTag());
    reregister_with_cluster_cache_(otag,
                                   otag);
}

TLogMultiplier
Volume::getEffectiveTLogMultiplier() const
{
    const boost::optional<TLogMultiplier> t(getTLogMultiplier());
    if (t)
    {
        return *t;
    }
    else
    {
        return TLogMultiplier(VolManager::get()->number_of_scos_in_tlog.value());
    }
}

void
Volume::wait_for_backend_and_run(std::function<void()> fun)
{
    auto t(std::make_unique<backend_task::FunTask>(*this,
                                                   std::move(fun),
                                                   yt::BarrierTask::T));

    VolManager::get()->backend_thread_pool()->addTask(std::move(t));
}

void
Volume::set_metadata_cache_capacity(const boost::optional<size_t>& num_pages)
{
    WLOCK();
    checkNotHalted_();

    const size_t new_capacity = num_pages ?
        *num_pages :
        VolManager::get()->metadata_cache_capacity.value();

    THROW_WHEN(new_capacity == 0);

    try
    {
        metaDataStore_->set_cache_capacity(new_capacity);
    }
    CATCH_STD_ALL_EWHAT({
            LOG_VERROR("Failed to update metadata store cache capacity: " << EWHAT);
            VolumeDriverError::report(events::VolumeDriverErrorCode::MetaDataStore,
                                      EWHAT,
                                      getName());
            halt();
            throw;
        });

    update_config_([&](VolumeConfig& cfg)
                   {
                       cfg.metadata_cache_capacity_ = num_pages;
                   });
}

size_t
Volume::effective_metadata_cache_capacity() const
{
    return VolManager::get()->effective_metadata_cache_capacity(get_config());
}

void
Volume::add_to_cluster_cache_(const ClusterCacheMode ccmode,
                              const ClusterAddress ca,
                              const youtils::Weed& weed,
                              const uint8_t* buf)
{
    // For now we only use the cache if it has the same cluster size. We could
    // try harder and split volume clusters into smaller cache clusters.
    ClusterCache& cache = VolManager::get()->getClusterCache();

    if ((ClusterLocationAndHash::use_hash() or
         ccmode != ClusterCacheMode::ContentBased) and
        cache.cluster_size() == getClusterSize())
    {
        cache.add(getClusterCacheHandle(),
                  ca,
                  weed,
                  buf,
                  static_cast<size_t>(getClusterSize()));
    }
}

void
Volume::purge_from_cluster_cache_(const ClusterAddress ca,
                                  const youtils::Weed& weed)
{
    // For now we only use the cache if it has the same cluster size. We could
    // try harder and split volume clusters into smaller cache clusters.
    ClusterCache& cache = VolManager::get()->getClusterCache();

    if (cache.cluster_size() == getClusterSize())
    {
        cache.invalidate(getClusterCacheHandle(),
                         ca,
                         weed);
    }
}

bool
Volume::find_in_cluster_cache_(const ClusterCacheMode ccmode,
                               const ClusterAddress ca,
                               const youtils::Weed& weed,
                               uint8_t* buf)
{
    // For now we only use the cache if it has the same cluster size. We could
    // try harder and split volume clusters into smaller cache clusters.
    ClusterCache& cache = VolManager::get()->getClusterCache();

    if ((ClusterLocationAndHash::use_hash() or
         ccmode != ClusterCacheMode::ContentBased) and
        cache.cluster_size() == getClusterSize())
    {
        return cache.read(getClusterCacheHandle(),
                          ca,
                          weed,
                          buf,
                          static_cast<size_t>(getClusterSize()));
    }
    else
    {
        return false;
    }
}

}

// Local Variables: **
// mode: c++ **
// End: **
