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

#include "CombinedTLogReader.h"
#include "DataStoreNG.h"
#include "BackendTasks.h"
#include "FailOverCacheConfig.h"
#include "SCOAccessData.h"
#include "Scrubber.h"
#include "ScrubWork.h"
#include "SnapshotManagement.h"
#include "TLogReader.h"
#include "TransientException.h"
#include "VolManager.h"
#include "WriteOnlyVolume.h"

#include <float.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <fstream>

#include <boost/filesystem/fstream.hpp>
#include <boost/scope_exit.hpp>

#include <youtils/Assert.h>
#include <youtils/FileUtils.h>
#include <youtils/IOException.h>
#include <youtils/System.h>
#include <youtils/Timer.h>
#include <youtils/UUID.h>

#include <backend/BackendInterface.h>

// sanity check
static_assert(FLT_RADIX == 2, "Need to check code for non conforming FLT_RADIX");

#define WLOCK()                                 \
    fungi::ScopedWriteLock swl__(rwlock_)

#define RLOCK()                                 \
    fungi::ScopedReadLock srl__(rwlock_)

#define SERIALIZE_WRITES()                              \
    boost::lock_guard<lock_type> gwl__(write_lock_)

#ifndef NDEBUG

#define ASSERT_WLOCKED()                        \
    VERIFY(not rwlock_.tryReadLock())

#define ASSERT_RLOCKED()                        \
    VERIFY(not rwlock_.tryWriteLock())

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

WriteOnlyVolume::WriteOnlyVolume(const VolumeConfig& vCfg,
                                 const OwnerTag owner_tag,
                                 std::unique_ptr<SnapshotManagement> snapshotManagement,
                                 std::unique_ptr<DataStoreNG> datastore,
                                 NSIDMap nsidmap)
    : write_lock_()
    , rwlock_(vCfg.id_ + "-volume-rwlock")
    , halted_(false)
    , dataStore_(datastore.release())
    , clusterSize_(vCfg.getClusterSize())
    , cfg_(vCfg)
    , snapshotManagement_(snapshotManagement.release())
    , volOffset_(ilogb(vCfg.lba_size_ ))
    , nsidmap_(std::move(nsidmap))
    , cluster_locations_(vCfg.sco_mult_)
    , volumeStateSpinLock_()
    , datastore_throttle_usecs_(VolManager::get()->getSCOCache()->datastore_throttle_usecs.value())
    , has_dumped_debug_data(false)
{
    WLOCK();

    LOG_INFO("Constructor of WriteOnlyVolume for namespace " << vCfg.getNS());

    VERIFY(vCfg.lba_size_ == exp2(volOffset_));

    maybe_update_owner_tag_(owner_tag);

    caMask_ = ~(getClusterSize() / getLBASize() - 1);
    dataStore_->initialize(this);
    snapshotManagement_->initialize(this);
}

void
WriteOnlyVolume::maybe_update_owner_tag_(const OwnerTag owner_tag)
{
    if (cfg_.owner_tag_ != owner_tag)
    {
        LOG_INFO(cfg_.id_ << "owner Tag has changed from " << cfg_.owner_tag_ <<
                 owner_tag);
        cfg_.owner_tag_ = owner_tag;
        writeConfigToBackend_();
    }
}

void
WriteOnlyVolume::tlogWrittenToBackendCallback(const TLogId& tid,
                                              const SCO sconame)
{
    snapshotManagement_->tlogWrittenToBackendCallback(tid,
                                                      sconame);
}

// called from mgmt thread
WriteOnlyVolume*
WriteOnlyVolume::newWriteOnlyVolume()
{
    WLOCK();

    const uint64_t max_non_disposable =
        VolManager::get()->get_sco_cache_max_non_disposable_bytes(cfg_);

    dataStore_->newVolume(0,
                          max_non_disposable);

    snapshotManagement_->scheduleWriteSnapshotToBackend();
    writeConfigToBackend_();

    // This used to be writeFailOverCacheConfigToBackend_
    fs::path tmp = FileUtils::create_temp_file_in_temp_dir("write_only_cache_config");
    ALWAYS_CLEANUP_FILE(tmp);

    FailOverCacheConfigWrapper foc_config_wrapper;
    youtils::Serialization::serializeAndFlush<FailOverCacheConfigWrapper::oarchive_type>(tmp,
                                                                                         foc_config_wrapper);

    try
    {
        getBackendInterface()->write(tmp,
                                     FailOverCacheConfigWrapper::config_backend_name,
                                     OverwriteObject::T,
                                     nullptr,
                                     backend_write_condition());
    }
    catch (be::BackendUniqueObjectTagMismatchException&)
    {
        LOG_WARN(getName() << ": conditional write of " <<
                 FailOverCacheConfigWrapper::config_backend_name << " failed");
        halt();
        throw;
    }

    return this;
}

// called from mgmt thread
WriteOnlyVolume*
WriteOnlyVolume::backend_restart(const SCONumber lastSCOInBackend)
{

    SERIALIZE_WRITES();
    WLOCK();

    const uint64_t max_non_disposable =
        VolManager::get()->get_sco_cache_max_non_disposable_bytes(cfg_);

    dataStore_->backend_restart(lastSCOInBackend,
                                0,
                                max_non_disposable);

    snapshotManagement_->scheduleWriteSnapshotToBackend();
    return this;
}

WriteOnlyVolume::~WriteOnlyVolume()
{
    WLOCK();

    LOG_INFO("Destructor of WriteOnlyVolume: " << getName() << " at " << this);
    VolManager::get()->backend_thread_pool()->stop(this);
}

// Y42 redo these, they should just be a right shift of lba by an amount
ClusterAddress WriteOnlyVolume::addr2CA(const uint64_t addr) const
{
    return addr / getClusterSize();
}

uint64_t WriteOnlyVolume::LBA2Addr(const uint64_t lba) const
{
    return (lba & caMask_) * getLBASize();
}

void
WriteOnlyVolume::validateIOLength(uint64_t lba, uint64_t len) const
{
    if (lba + len / getLBASize() > getLBACount())
    {
        LOG_WARN("Access beyond end of volume: LBA "
                 << (lba + len / getLBASize()) << " > "
                 << getLBACount());
        // don't throw return boolean!!
        throw fungi::IOException("Access beyond end of WriteOnlyVolume");
    }
}

void
WriteOnlyVolume::validateIOAlignment(uint64_t lba, uint64_t len) const
{
    if (len % getClusterSize())
    {
        LOG_WARN("Access not aligned to cluster size: len " << len <<
                 ", clustersize " << getClusterSize());
        throw fungi::IOException("Access not aligned to cluster size");
    }
    if (lba & ~caMask_)
    {
        LOG_WARN("LBA " << lba << " not aligned to cluster (mask: " <<
                 std::hex << ~caMask_ << std::dec);
        throw fungi::IOException("LBA not aligned to cluster");
    }
}

void
WriteOnlyVolume::writeClusterMetaData_(ClusterAddress ca,
                                       const ClusterLocationAndHash& loc_and_hash)
{
    ASSERT_WRITES_SERIALIZED();
    ASSERT_WLOCKED();
    snapshotManagement_->addClusterEntry(ca,
                                         loc_and_hash);
}


void
WriteOnlyVolume::write(uint64_t lba,
                       const uint8_t *buf,
                       uint64_t buflen)
{
    LOG_DEBUG("lba " << lba << ", len " << buflen << ", buf " << &buf);
    performance_counters().write_request_size.count(buflen);
    yt::SteadyTimer t;

    checkNotHalted_();
    //MeasureScopeInt ms("Volume::write", lba);
    validateIOLength(lba, buflen);

    uint64_t addrOffset = (lba & ~caMask_) * getLBASize();
    uint64_t len = intCeiling(addrOffset + buflen, getClusterSize());

    bool unaligned = (lba & ~caMask_) != 0 ||
                     buflen % getClusterSize() != 0;

    std::vector<uint8_t> bufv;
    const uint8_t *p;
    //uint64_t alignedLBA = lba & caMask_;

    if (unaligned)
    {
        LOG_ERROR("Write Only volumes don't do unaligned writes");
        throw UnalignedWriteException("Unaligned write on write only volume");

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
        const ssize_t sco_cap = dataStore_->getRemainingSCOCapacity();

        // we need to guarantee that ourselves by forcing a SCO rollover
        // when updating the SCOMultiplier
        VERIFY(sco_cap >= 0); // can we really deal with sco_cap == 0?
        const size_t chunksize = std::min(wsize,
                                          static_cast<size_t>(sco_cap) * getClusterSize());

        writeClusters_(addr + off, p + off, chunksize);
        off += chunksize;
        wsize -= chunksize;
    }

    const auto duration_us(bc::duration_cast<bc::microseconds>(t.elapsed()));
    performance_counters().write_request_usecs.count(duration_us.count());

    VERIFY(off == len);
}

void
WriteOnlyVolume::writeClusters_(uint64_t addr,
                                const uint8_t* buf,
                                uint64_t bufsize)
{
    ASSERT_WRITES_SERIALIZED();

    VERIFY(bufsize % getClusterSize() == 0);

    size_t num_locs = bufsize / getClusterSize();
    unsigned throttle_usecs = 0;
    uint32_t ds_throttle = 0;

    {
        // prevent concurrent writes and tlog rollover interfering with snapshotting
        // and friends
        WLOCK();
        dataStore_->writeClusters(buf,
                                  cluster_locations_,
                                  num_locs,
                                  ds_throttle);

        for (size_t i = 0; i < num_locs; ++i)
        {
            const uint8_t* data = buf + i * getClusterSize();
            uint64_t clusteraddr = addr + i * getClusterSize();
            ClusterAddress ca = addr2CA(clusteraddr);
            ClusterLocationAndHash loc_and_hash(cluster_locations_[i],
                                                data,
                                                getClusterSize());

            writeClusterMetaData_(ca,
                                  loc_and_hash);

            LOG_DEBUG("lba " << (clusteraddr / getLBASize()) <<
                      " CA " << cluster_locations_[i]);
        }

        const ssize_t sco_cap = dataStore_->getRemainingSCOCapacity();
        VERIFY(sco_cap >= 0);

        if (sco_cap == 0)
        {
            LOG_DEBUG(getName() << ": requesting SCO rollover, adding CRC to tlog");
            MaybeCheckSum cs = dataStore_->finalizeCurrentSCO();
            VERIFY(cs);
            snapshotManagement_->addSCOCRC(*cs);
        }
    }

    if (ds_throttle > 0)
    {
        const unsigned ds_throttle_usecs = ds_throttle * num_locs;
        throttle_usecs = ds_throttle_usecs - std::min(ds_throttle_usecs,
                                                      throttle_usecs);

        throttle_(throttle_usecs);
    }
}

uint64_t
WriteOnlyVolume::getSize() const
{
    return getLBACount() * getLBASize();
}

uint64_t
WriteOnlyVolume::getLBASize() const
{
    return cfg_.lba_size_;
}

uint64_t
WriteOnlyVolume::getLBACount() const
{
    return cfg_.lba_count();
}


const VolumeId
WriteOnlyVolume::getName() const
{
    return cfg_.id_;
}

backend::Namespace
WriteOnlyVolume::getNamespace() const
{
    return cfg_.getNS();
}

VolumeConfig
WriteOnlyVolume::get_config() const
{
    return cfg_;
}

void WriteOnlyVolume::createSnapshot(const SnapshotName& name,
                                     const SnapshotMetaData& metadata,
                                     const UUID& uuid)
{
    WLOCK(); // Z42: make more fine grained?

    checkNotHalted_();

    if (not snapshotManagement_->lastSnapshotOnBackend())
    {
        LOG_ERROR("Last snapshot not on backend");
        throw fungi::IOException("Last snapshot not on backend, can't create a new one");
    }

    // TODO: halt the volume in case of non-recoverable errors
    sync_(AppendCheckSum::F);

    const MaybeCheckSum cs(dataStore_->finalizeCurrentSCO());
    snapshotManagement_->createSnapshot(name,
                                        cs,
                                        metadata,
                                        uuid,
                                        true);
}

bool
WriteOnlyVolume::checkSnapshotUUID(const SnapshotName& snapshotName,
                                   const volumedriver::UUID& uuid) const
{
    return snapshotManagement_->checkSnapshotUUID(snapshotName,
                                                  uuid);
}

bool
WriteOnlyVolume::snapshotExists(const SnapshotName& name) const
{
    return snapshotManagement_->snapshotExists(name);
}

void
WriteOnlyVolume::listSnapshots(std::list<SnapshotName>& snapshots) const
{
    snapshotManagement_->listSnapshots(snapshots);
}

Snapshot
WriteOnlyVolume::getSnapshot(const SnapshotName& snapname) const
{
    return snapshotManagement_->getSnapshot(snapname);
}

void
WriteOnlyVolume::deleteSnapshot(const SnapshotName& name)
{
    checkNotHalted_();
    snapshotManagement_->deleteSnapshot(name);
}

fs::path
WriteOnlyVolume::ensureDebugDataDirectory()
{
    fs::path directory = VolManager::get()->debug_metadata_path.value();
    fs::create_directories(directory);

    std::stringstream ss;
    ss << time(0);

    std::string filename = getName() + "_debugdata_for_halted_volume_" + ss.str() + ".tar.gz";

    return directory / filename;
}

void
WriteOnlyVolume::dumpDebugData()
{
    if(not has_dumped_debug_data)
    {
        try
        {
            const fs::path path = ensureDebugDataDirectory();
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
            LOG_WARN("Could not dump volume debug data, " << e.what());
        }
        catch(...)
        {
            LOG_WARN("Could not dump WriteOnlyVolume debug data, unknown exception");
        }
    }
    else
    {
    }
}

// TODO: this only works when the backend is online
void WriteOnlyVolume::restoreSnapshot(const SnapshotName& name)
{
    WLOCK();

    const SnapshotNum num = snapshotManagement_->getSnapshotNumberByName(name);

    if(not snapshotManagement_->isSnapshotInBackend(num))
    {
        LOG_ERROR("Not restoring snapshot " << name << " because it is not in the backend yet");
        throw fungi::IOException("Not restoring snapshot because it is not in the backend yet");
    }

    sync_(AppendCheckSum::F);

    LOG_INFO("Removing all tasks from the threadpool for this WriteOnlyVolume");
    try
    {
        VolManager::get()->backend_thread_pool()->stop(this, 10);

        // 1) update metadata store => not applicable

        // 2) get list of excess SCOs - to be removed only after persisting the new snapshots file
        std::vector<SCO> doomedSCOs;
        OrderedTLogIds doomedTLogs;

        try
        {
            LOG_INFO("Finding out which TLogs and SCO's to delete");

            doomedTLogs = snapshotManagement_->getTLogsAfterSnapshot(num);

            LOG_INFO("Deleting " << doomedTLogs.size() << " TLogs");

            const fs::path path(VolManager::get()->getTLogPath(*this));
            std::shared_ptr<TLogReaderInterface> r = CombinedTLogReader::create(path,
                                                                                doomedTLogs,
                                                                                getBackendInterface()->clone());

            r->SCONames(doomedSCOs);
            LOG_INFO("Deleting " << doomedSCOs.size() << " SCOs");
        }
        catch(std::exception& e)
        {
            LOG_ERROR("Could not get all SCO's that can be deleted from the backend: " << e.what());
        }
        catch(...)
        {
            LOG_ERROR("Could not get all SCO's that can be deleted from the backend: Unknown Exception");
        }

        // 3) persist snapshots file and cleanup TLogs
        snapshotManagement_->eraseSnapshotsAndTLogsAfterSnapshot(num);

        LOG_INFO("Finding out last sco on backend for WriteOnlyVolume " << getName());

        const OrderedTLogIds tlog_ids(snapshotManagement_->getAllTLogs());
        std::shared_ptr<TLogReaderInterface> itf =
            CombinedTLogReader::create_backward_reader(snapshotManagement_->getTLogsPath(),
                                                       tlog_ids,
                                                       getBackendInterface()->clone());

        ClusterLocation last_in_backend = itf->nextClusterLocation();
        LOG_INFO("Resetting datastore to " << last_in_backend <<
                 " for WriteOnlyVolume " << getName());
        dataStore_->restoreSnapshot(last_in_backend.number());

        // 4) reset the FOC
        LOG_INFO("Not Resetting the FOC for WriteOnlyVolume " << getName());


        // 6) remove the doomed TLogs
        for (const auto& tlog_id : doomedTLogs)
        {

            backend_task::DeleteTLog* task = new backend_task::DeleteTLog(this,
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
        // Y42 check this
        VolManager::get()->backend_thread_pool()->addTask(new backend_task::Barrier(this));
    }
    catch(std::exception& e)
    {
        LOG_ERROR("Snapshot Restore failed, setting WriteOnlyVolume " << getName() << " halted " << e.what());
        halt();
        throw;
    }
    catch(...)
    {
        LOG_ERROR("Snapshot Restore failed, setting WriteOnlyVolume " << getName() << " halted, unknown exception ");
        halt();
        throw;
    }

}


void
WriteOnlyVolume::destroy(const RemoveVolumeCompletely,
                         uint64_t timeout)
{
    WLOCK();
#define CATCH_AND_IGNORE(x, message)            \
    try                                         \
    {                                           \
        x;                                      \
    }                                           \
    catch(std::exception& e)                    \
    {                                           \
        LOG_WARN(message << e.what());          \
    }

    LOG_INFO("destroying WriteOnlyVolume " << getName());



    CATCH_AND_IGNORE(VolManager::get()->backend_thread_pool()->stop(this,
                                                                    timeout),
                     "Exception trying to stop the backend threadpool");

    CATCH_AND_IGNORE(snapshotManagement_->destroy(DeleteLocalData::T),
                          "Exception trying to destroy snapshotmanagment");

    // Do we want to put this into
     try
     {
        dataStore_->destroy(DeleteLocalData::T);
     }
     catch (std::exception &e)
     {
         LOG_ERROR("failed to remove DataStore: " << e.what() <<
                   ". Manual cleanup required");
     }

    CATCH_AND_IGNORE(fs::remove_all(VolManager::get()->getMetaDataPath(*this)),
                          "Exception trying to delete the medata directory");

    CATCH_AND_IGNORE(fs::remove_all(VolManager::get()->getTLogPath(*this)),
                              "Exception tyring to delete the tlog directory");
}

bool
WriteOnlyVolume::isSyncedToBackend() const
{
    return (not halted_) and
        snapshotManagement_->isSyncedToBackend()  and
        VolManager::get()->backend_thread_pool()->noTasksPresent(const_cast<WriteOnlyVolume*>(this));
}

bool
WriteOnlyVolume::isSyncedToBackendUpTo(const SnapshotName& snapshotName) const
{
    SnapshotNum num = snapshotManagement_->getSnapshotNumberByName(snapshotName);
    return snapshotManagement_->isSnapshotInBackend(num);
}

void
WriteOnlyVolume::getScrubbingWork(std::vector<std::string>& scrub_work,
                                  const boost::optional<SnapshotName>& start_snap,
                                  const boost::optional<SnapshotName>& end_snap) const
{
    SnapshotWork work;

    snapshotManagement_->getSnapshotScrubbingWork(start_snap,
                                                  end_snap,
                                                  work);

    const VolumeConfig cfg(get_config());

    for(const auto& w : work)
    {
        scrubbing::ScrubWork s(VolManager::get()->getBackendConfig().clone(),
                               cfg.getNS(),
                               cfg.id_,
                               ilogb(cfg.cluster_mult_ * cfg.lba_size_),
                               cfg.sco_mult_,
                               w);

        scrub_work.push_back(s.str());
    }
}

SnapshotName
WriteOnlyVolume::getParentSnapName() const
{
    return cfg_.parent_snapshot_;
}


uint64_t
WriteOnlyVolume::VolumeDataStoreWriteUsed() const
{
    return dataStore_->getWriteUsed();
}

uint64_t
WriteOnlyVolume::VolumeDataStoreReadUsed() const
{
    return dataStore_->getReadUsed();
}

uint64_t
WriteOnlyVolume::getTLogUsed() const
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
WriteOnlyVolume::scheduleBackendSync()
{
    WLOCK();
    checkNotHalted_();

    // TODO: halt the volume in case of non-recoverable errors
    sync_(AppendCheckSum::F);

    MaybeCheckSum cs = dataStore_->finalizeCurrentSCO();
    snapshotManagement_->scheduleBackendSync(cs);
}

uint64_t
WriteOnlyVolume::getSnapshotSCOCount(const SnapshotName& snapshotName)
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
WriteOnlyVolume::getCacheHits() const
{
    return dataStore_->getCacheHits();
}

uint64_t
WriteOnlyVolume::getCacheMisses() const
{
    return dataStore_->getCacheMisses();
}

uint64_t
WriteOnlyVolume::getNonSequentialReads() const
{
    // Z42: reinstate once we've got a good idea what to measure :)
    return 0;
}

void
WriteOnlyVolume::sync()
{
    WLOCK();
    sync_(AppendCheckSum::F);
}

void
WriteOnlyVolume::sync_(AppendCheckSum append_chksum)
{
    ASSERT_WLOCKED();

    LOG_INFO("Syncing volume " << getName());

    checkNotHalted_();
    const auto maybe_sco_crc(dataStore_->sync());
    // Y42 Check This
    snapshotManagement_->sync(append_chksum == AppendCheckSum::T ?
                              maybe_sco_crc :
                              boost::none);
}

bool
WriteOnlyVolume::checkSCONamesConsistency_(const std::vector<SCO>& names)
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
WriteOnlyVolume::checkTLogsConsistency_(CloneTLogs& ctl) const
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
                       bi->clone()).SCONames(scoNames);;

            if (not (scoNames.empty() or
                     (scoNames.front().number() > lastSCONumber)))
            {
                LOG_ERROR("SCO number at beginning of tlog "
                          << tlogs[k] << "has not increased compared to last SCO from previous tlog");
                return false;
            }

            if (not checkSCONamesConsistency_(scoNames))
            {
                LOG_ERROR("SCO names are not consistent");

                return false;
            }

            for (unsigned i = 0; i < scoNames.size(); i++)
            {
                const std::string name = scoNames[i].str();
                if (not bi->objectExists(name))
                {
                    LOG_ERROR("SCO " << name << " in tlog " << tlogs[k] << " does not exist on backend");

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

uint64_t
WriteOnlyVolume::getSnapshotBackendSize(const SnapshotName& snapName)
{
    return snapshotManagement_->getSnapshotBackendSize(snapName);
}

uint64_t
WriteOnlyVolume::getCurrentBackendSize() const
{
    return snapshotManagement_->getCurrentBackendSize();
}

uint64_t
WriteOnlyVolume::getTotalBackendSize() const
{
    return snapshotManagement_->getTotalBackendSize();
}

void
WriteOnlyVolume::SCOWrittenToBackendCallback(uint64_t file_size,
                                             bc::microseconds write_time)
{
    performance_counters().backend_write_request_size.count(file_size);
    performance_counters().backend_write_request_usecs.count(write_time.count());
}

void
WriteOnlyVolume::normalizeSAPs_(SCOAccessData::VectorType& sadv)
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
WriteOnlyVolume::writeConfigToBackend_()
{
    fs::path tmp = VolManager::get()->getMetaDataPath(*this) / VolumeConfig::config_backend_name;
    FileUtils::removeFileNoThrow(tmp);
    VERIFY(not fs::exists(tmp));

    ALWAYS_CLEANUP_FILE(tmp);
    youtils::Serialization::serializeAndFlush<VolumeConfig::oarchive_type>(tmp, cfg_);

    try
    {
        getBackendInterface()->write(tmp,
                                     VolumeConfig::config_backend_name,
                                     OverwriteObject::T,
                                     nullptr,
                                     backend_write_condition());
    }
    catch (be::BackendUniqueObjectTagMismatchException&)
    {
        LOG_WARN(getName() << ": conditional write of " <<
                 VolumeConfig::config_backend_name << " failed");
        halt();
        throw;
    }
}

fs::path
WriteOnlyVolume::getTempTLogPath() const
{
    // Y42 maybe just create this once in the constructor of the volume
    fs::path p = snapshotManagement_->getTLogsPath();
    p /= "tmp";
    fs::create_directories(p);
    return p;
}

fs::path
WriteOnlyVolume::getCurrentTLogPath() const
{
    return snapshotManagement_->getTLogsPath();
}

const SnapshotManagement&
WriteOnlyVolume::getSnapshotManagement() const
{
    return *snapshotManagement_;
}

void
WriteOnlyVolume::halt()
{
    VolumeDriverError::report(events::VolumeDriverErrorCode::VolumeHalted,
                              "halting volume",
                              getName());

    LOG_FATAL("Setting volume " << getName() << " to 'halted'");
    halted_ = true;
    try
    {
        VolManager::get()->backend_thread_pool()->stop(this, 10);
    }
    catch (std::exception& e)
    {
        LOG_ERROR(getName() << ": failed to clear volume's queue: " << e.what() <<
                  " - ignoring");
    }
    catch (...)
    {
        LOG_ERROR(getName() << ": failed to clear WriteOnlyVolume's queue: unknown error - ignoring");
    }

    try
    {
        VolManager::get()->backend_thread_pool()->halt(this, 10);
    }
    catch (...)
    {
        LOG_ERROR(getName() << ": failed to stop WriteOnlyVolume's queue: unknown error - ignoring");
    }
    dumpDebugData();

}

bool
WriteOnlyVolume::is_halted() const
{
    return halted_;
}

fs::path
WriteOnlyVolume::saveSnapshotToTempFile()
{
    return snapshotManagement_->saveSnapshotToTempFile();
}

void
WriteOnlyVolume::checkNotHalted_() const
{
    if (halted_)
    {
        throw fungi::IOException("WriteOnlyVolume is halted due to previous errors",
                                 getName().c_str());
    }
}

void
WriteOnlyVolume::throttle_(unsigned throttle_usecs) const
{
    LOG_TRACE("Not throttling a write only volume for " << throttle_usecs << " microseconds");
}

void
WriteOnlyVolume::cork(const youtils::UUID&)
{}

void
WriteOnlyVolume::unCorkAndTrySync(const youtils::UUID&)
{}

void
WriteOnlyVolume::metaDataBackendConfigHasChanged(const MetaDataBackendConfig&)
{
    VERIFY(0 == "I don't have a metadata store so how can its config have changed?");
}

}

// Local Variables: **
// mode: c++ **
// End: **
