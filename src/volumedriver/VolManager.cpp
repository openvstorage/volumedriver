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

#include "BackendTasks.h"
#include "Entry.h"
#include "LockStoreFactory.h"
#include "SCOCache.h"
#include "SCOCacheAccessDataPersistor.h"
#include "SnapshotManagement.h"
#include "Types.h"
#include "VolManager.h"
#include "Volume.h"
#include "VolumeConfigPersistor.h"

#include "metadata-server/Manager.h"

#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <signal.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <list>

#include <stdexcept>
#include <sstream>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/scope_exit.hpp>

#include <youtils/Assert.h>
#include <youtils/Catchers.h>
#include <youtils/DimensionedValue.h>
#include <youtils/MainEvent.h>
#include <youtils/PeriodicAction.h>
#include <youtils/VolumeDriverComponent.h>

#include <backend/BackendConfig.h>
#include <backend/BackendConnectionManager.h>
#include <backend/BackendInterface.h>
#include <backend/GarbageCollector.h>
#include <backend/Local_Connection.h>

namespace volumedriver
{

namespace be = backend;
namespace bpt = boost::property_tree;
namespace fs = boost::filesystem;
namespace yt = youtils;

using namespace initialized_params;
using ::youtils::Notifier;

VolManager* VolManager::instance_ = 0;
fungi::Mutex VolManager::instanceMutex_("VolManagerInstanceMutex");

#define LOCK_MANAGER()                          \
    fungi::ScopedLock __l(mgmtMutex_)

#define UNLOCK_MANAGER()                        \
    ScopedUnlock __u(mgmtMutex_)

// push to Utilties
namespace
{

class ScopedUnlock
{
public:
    explicit ScopedUnlock(fungi::Mutex& m)
        : m_(m)
    {
        m_.assertLocked();
        m_.unlock();
    }

    ScopedUnlock(const ScopedUnlock&) = delete;
    ScopedUnlock& operator=(const ScopedUnlock&) = delete;

    ~ScopedUnlock()
    {
        m_.lock();
    }

private:
    fungi::Mutex& m_;
};

}

VolManager::VolManager(const boost::property_tree::ptree& pt,
                       events::PublisherPtr event_publisher)
try
    : VolumeDriverComponent(RegisterComponent::T, pt)
          , mgmtMutex_("VolMgmtMutex", fungi::Mutex::ErrorCheckingMutex)
          , event_publisher_(event_publisher)
          , backend_thread_pool_(pt)
          , read_activity_time_(::time(0))
          , scoCache_(pt)
          , readOnlyMode_(false)
          , asio_svc_mgr_component_(pt,
                                    RegisterComponent::T)
          , backend_conn_manager_(be::BackendConnectionManager::create(pt))
          , backend_garbage_collector_(std::make_shared<be::GarbageCollector>(backend_conn_manager_,
                                                                              pt,
                                                                              RegisterComponent::T))
          , lock_store_factory_(std::make_unique<LockStoreFactory>(pt,
                                                                   RegisterComponent::T,
                                                                   backend_conn_manager_))
          , ClusterCache_(pt,
                          ClusterSize(decltype(default_cluster_size)(pt).value()),
                          RegisterComponent::T)
          , mds_manager_(std::make_shared<metadata_server::Manager>(pt,
                                                                    RegisterComponent::T,
                                                                    backend_conn_manager_))
          , max_file_descriptors_(boost::none)
          , metadata_path(pt)
          , tlog_path(pt)
          , open_scos_per_volume(pt)
          , freespace_check_interval(pt)
          , dtl_check_interval_in_seconds(pt)
          , read_cache_default_behaviour(pt)
          , read_cache_default_mode(pt)
          , sco_written_to_backend_action(pt)
          , clean_interval(pt)
          , sap_persist_interval(pt)
          , required_tlog_freespace(pt)
          , required_meta_freespace(pt)
          , max_volume_size(pt)
          , dtl_throttle_usecs(pt)
          , dtl_queue_depth(pt)
          , dtl_write_trigger(pt)
          , dtl_busy_loop_usecs(pt)
          , dtl_request_timeout_ms(pt)
          , dtl_connect_timeout_ms(pt)
          , number_of_scos_in_tlog(pt)
          , non_disposable_scos_factor(pt)
          , default_cluster_size(pt)
          , metadata_cache_capacity(pt)
          , metadata_mds_slave_max_tlogs_behind(pt)
          , debug_metadata_path(pt)
          , arakoon_metadata_sequence_size(pt)
          , allow_inconsistent_partial_reads(pt)
          , volume_nullio(pt)
{
    THROW_UNLESS((default_cluster_size.value() % VolumeConfig::default_lba_size()) == 0);

    periodicActions_.push_back(new yt::PeriodicAction("SCOCacheCleaner",
                                                      [this]
                                                      {
                                                          scoCache_.cleanup();
                                                      },
                                                      clean_interval.value()));

    periodicActions_.push_back(new yt::PeriodicAction("SCOCacheAccessDataPersistor",
                                                      [this]
                                                      {
                                                          SCOCacheAccessDataPersistor p(scoCache_);
                                                          p();
                                                      },
                                                      sap_persist_interval.value()));

    periodicActions_.push_back(new yt::PeriodicAction("MetaDataFreeSpaceChecker",
                                                      [this]
                                                      {
                                                          checkMetaDataFreeSpace();
                                                      },
                                                      freespace_check_interval.value()));

    periodicActions_.push_back(new yt::PeriodicAction("FailOverCacheChecker",
                                                      [this]
                                                      {
                                                          checkVolumeFailoverCaches();
                                                      },
                                                      dtl_check_interval_in_seconds.value()));
}
CATCH_STD_ALL_LOG_RETHROW("Exception during VolManager construction");

void
VolManager::start(const fs::path& path,
                  events::PublisherPtr publisher)
{
    boost::property_tree::ptree pt;
    boost::property_tree::json_parser::read_json(path.string(),
                                                 pt);
    start(pt,
          publisher);
}

void
VolManager::start(const boost::property_tree::ptree& pt,
                  events::PublisherPtr publisher)
{
    if (instance_)
    {
        LOG_ERROR("Voldriver already started");
    }
    else
    {
        try
        {
            LOG_INFO("Initializing: should only happen on startup or in the testers");

            FileUtils::ensure_directory(PARAMETER_TYPE(tlog_path)(pt).value());
            FileUtils::ensure_directory(PARAMETER_TYPE(metadata_path)(pt).value());

            instance_ = new VolManager(pt,
                                       publisher);
            LOG_INFO("Finished Initializing.");
        }
        CATCH_STD_ALL_LOG_RETHROW("Problem creating VolManager");
    }
}

VolManager*
VolManager::get()
{
    if (not instance_)
    {
        LOG_ERROR("Voldriver not started");
        throw std::logic_error("VolManager is not running");
    }

    return instance_;
}

bool
VolManager::updateConfiguration(const boost::property_tree::ptree& pt,
                                UpdateReport& report,
                                ConfigurationReport& conf_report)
{
    if(checkConfiguration(pt,
                          conf_report))
    {
        for (auto comp : Notifier::components)
        {
            TODO("AR: make ->update noexcept!?");
            LOG_INFO("Updating Component " << comp->componentName());
            comp->update(pt,
                         report);
        }
        return true;
    }

    return false;
}

boost::variant<youtils::UpdateReport,
               youtils::ConfigurationReport>
VolManager::updateConfiguration(const fs::path& path)
{
    boost::property_tree::ptree pt;
    boost::property_tree::json_parser::read_json(path.string(),
                                                 pt);
    return updateConfiguration(pt);
}

boost::variant<youtils::UpdateReport,
               youtils::ConfigurationReport>
VolManager::updateConfiguration(const bpt::ptree& pt)
{
    youtils::UpdateReport urep;
    youtils::ConfigurationReport crep;

    const bool res = updateConfiguration(pt, urep, crep);
    if (res)
    {
        return urep;
    }
    else
    {
        return crep;
    }
}

bool
VolManager::checkConfiguration(const boost::filesystem::path& path,
                               ConfigurationReport& c_rep)
{
    boost::property_tree::ptree pt;
    boost::property_tree::json_parser::read_json(path.string(),
                                                 pt);
    return checkConfiguration(pt,
                              c_rep);
}

bool
VolManager::checkConfiguration(const boost::property_tree::ptree& pt,
                               ConfigurationReport& conf)
{

    bool result = true;

    for (auto comp : Notifier::components)
    {
        LOG_INFO("Checking Component " << comp->componentName());
        result = comp->checkConfig(pt,
                                   conf) and result;
    }

    return result;
}

void
VolManager::persistConfiguration(boost::property_tree::ptree& pt,
                                 const ReportDefault reportDefault) const
{
    for (auto comp : Notifier::components)
    {
        LOG_INFO("Persisting Component " << comp->componentName());
        comp->persist(pt,
                      reportDefault);
    }
}

void
VolManager::persistConfiguration(const fs::path& path,
                                 const ReportDefault reportDefault) const
{
    boost::property_tree::ptree pt;

    persistConfiguration(pt,
                         reportDefault);
    boost::property_tree::json_parser::write_json(path.string(),
                                                  pt);
}

std::string
VolManager::persistConfiguration(const ReportDefault reportDefault) const
{
    boost::property_tree::ptree pt;
    persistConfiguration(pt,
                         reportDefault);
    std::stringstream ss;
    boost::property_tree::json_parser::write_json(ss,
                                                  pt);
    return ss.str();
}

void
VolManager::stop()
{
    // We need to protect here against double stops!!
    LOG_INFO("Stopping the Volume Manager: should only happen on shutdown");
    if(!Notifier::components.empty())
    {
        LOG_INFO("Cleaning up " << Notifier::components.size() << " components.");
        Notifier::components.clear();
    }

    if(not instance_)
    {
        LOG_ERROR("Destroying Volume Manager when instance_ is null");
        throw std::logic_error("VolManager is not running - cannot stop it");
    }

    delete instance_;
    instance_ = 0;
}

VolManager::~VolManager()
{
    // protect here against double entry
    LOG_INFO("stopping periodic actions");
    periodicActions_.clear();

    fungi::ScopedLock l(mgmtMutex_);
    try
    {
        LOG_INFO("stopping thread pool");
        backend_thread_pool_.stop();
    }
    catch (std::exception& e)
    {
        LOG_ERROR("error while trying to stop the thread pool: " <<
                  e.what());
    }
    catch (...)
    {
        LOG_ERROR("unknown error while trying to stop the thread pool");
    }


    for (VolumeMap::iterator it = volMap_.begin();
         it != volMap_.end(); ++it)
    {
        LOG_INFO("removing volume " << it->first);
        SharedVolumePtr vol = it->second;

        vol->destroy(DeleteLocalData::F,
                     RemoveVolumeCompletely::F,
                     ForceVolumeDeletion::F);
    }

    volMap_.clear();

    LOG_INFO("Exiting volumedriver destructor");

}


void
VolManager::scheduleTask(VolPoolTask* t)
{
  backend_thread_pool_.addTask(t);
}

void
VolManager::ensureNamespaceNotRestarting(const VolumeId& id) const
{
    for (const auto& p : restartMap_)
    {
        if (p.second.id_ == id)
        {
            LOG_ERROR("Volume restart of " << id << ", namespace " << p.first << " in progress");
            throw VolumeRestartInProgressException("Volume restart in progress",
                                                   id.str().c_str(),
                                                   EAGAIN);
        }
    }
}

void
VolManager::ensureNamespaceNotRestarting(const Namespace& ns) const
{
    auto it = restartMap_.find(ns);
    if (it != restartMap_.end())
    {
        const VolumeId& id = it->second.id_;
        LOG_ERROR("Volume restart of " << id << ", namespace " << ns <<
                  " in progress");
        throw VolumeRestartInProgressException("Volume restart in progress",
                                               id.str().c_str(),
                                               EAGAIN);
    }
}

uint64_t
VolManager::getCurrentVolumesTLogRequirements()
{
    mgmtMutex_.assertLocked();

    uint64_t res = 0;

    for (const VolumeMap::value_type& v : volMap_)
    {
        const uint64_t s = v.second->getSCOSize();
        const boost::optional<TLogMultiplier> t(v.second->getTLogMultiplier());

        LOG_TRACE("running volume: " << v.second->getName() <<
                  ", sco size: " << s <<
                  ", tlog multiplier: " << t);
        res += s * (t ? *t : number_of_scos_in_tlog.value());
    }

    for (const RestartMap::value_type& v : restartMap_)
    {
        const uint64_t s = v.second.getSCOSize();
        const boost::optional<TLogMultiplier> t(v.second.tlog_mult_);

        LOG_TRACE("restarting volume: " << v.second.id_ <<
                  ", sco size: " << s <<
                  ", tlog multiplier: " << t);

        // g++ (observed with 4.9) emits a false positive warning for this one in release builds,
        // cf. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=47679
        // /opt/vmachines/jenkins_work/workspace/volumedriver-no-dedup-compile-warnings-ubuntu-14.04/volumedriver-core/src/volumedriver/VolManager.cpp:486:18: warning: '*((void*)(& t)+4).volumedriver::TLogMultiplier::t' may be used uninitialized in this function [-Wmaybe-uninitialized]
        //  res += s * ((t == boost::none) ? number_of_scos_in_tlog.value() : t.get());
        //           ^
        // NB: still present with g++ 5.4.
#if defined(__GNUC__) &&                        \
    (__GNUC__ <= 4 ||                           \
     (__GNUC__ == 5 && GNUC_MINOR__ <= 4))
        PRAGMA_IGNORE_WARNING_BEGIN("-Wmaybe-uninitialized");
#endif
        res += s * (t ? *t : number_of_scos_in_tlog.value());
#if defined(__GNUC__) &&                        \
    (__GNUC__ <= 4 ||                           \
     (__GNUC__ == 5 && GNUC_MINOR__ <= 4))
        PRAGMA_IGNORE_WARNING_END
#endif
    }

    return res;
}

uint64_t
VolManager::getSCOCacheCapacityWithoutThrottling()
{
    uint64_t availableCacheSize = 0;
    SCOCacheMountPointsInfo info;
    scoCache_.getMountPointsInfo(info);

    for (const SCOCacheMountPointsInfo::value_type& v : info)
    {
        if (not v.second.offlined)
        {
            LOG_TRACE(v.second.path <<
                      ": capacity " << v.second.capacity <<
                      ", not available " << getSCOCache()->trigger_gap.value().getBytes());
            availableCacheSize += v.second.capacity - getSCOCache()->trigger_gap.value().getBytes();
        }
        else
        {
            LOG_TRACE(v.second.path << ": offline");
        }
    }
    return availableCacheSize;
}

uint64_t
VolManager::volumePotentialSCOCache(const ClusterSize cluster_size,
                                    const SCOMultiplier sco_mult,
                                    const boost::optional<TLogMultiplier>& tlog_mult)
{
    const uint64_t current_volume_usage = getCurrentVolumesTLogRequirements();
    const uint64_t current_scocache_capacity = getSCOCacheCapacityWithoutThrottling();
    const uint64_t sco_size = cluster_size * sco_mult;
    const uint64_t tlog_multiplier =
        tlog_mult ? *tlog_mult : number_of_scos_in_tlog.value();
    const uint64_t volume_potential =
        std::max(current_scocache_capacity - current_volume_usage,
                 (uint64_t)0) /
        (sco_size * tlog_multiplier);

    // LOG_INFO("current volume usage: " <<   requiredCacheSize
    //          << ", current_scocache_capacity: " << current_scocache_capacity);
    LOG_INFO("We have enough room for an additional " << volume_potential <<
             " volumes with cluster size " << cluster_size <<
             ", SCO multiplier " << sco_mult <<
             " (= SCO size " << sco_size <<
             "), TLog multiplier " << tlog_multiplier);

    return volume_potential;
}


namespace
{

DECLARE_LOGGER("VolManager");

uint64_t
getMaxFileDescriptors_()
{
    struct rlimit rl;
    int ret = ::getrlimit(RLIMIT_NOFILE, &rl);
    if (ret < 0)
    {
        ret = errno;
        LOG_ERROR("cannot determine open files limit: " << strerror(ret));
        throw fungi::IOException("Cannot determine open files limit");
    }

    LOG_INFO("Maximum number of open file descriptors: " << rl.rlim_cur
             << ", could be raised to " << rl.rlim_max);

    return rl.rlim_cur;
}
}

uint64_t
VolManager::getMaxFileDescriptors() const
{
    if(not max_file_descriptors_)
    {
        max_file_descriptors_ = getMaxFileDescriptors_();
    }
    return *max_file_descriptors_;
}

// TODO: push this to BackendConnectionManager
uint64_t
VolManager::backendFDEstimate() const
{
    switch (backend_conn_manager_->config().backend_type.value())
    {
    case backend::BackendType::LOCAL:
        return backend::local::Connection::lru_cache_size;
    case backend::BackendType::S3:
        return 0;
    case backend::BackendType::MULTI:
        return 0;
    case backend::BackendType::ALBA:
        return 0;
    }
    UNREACHABLE;
}

uint64_t
VolManager::globalFDEstimate() const
{
    // This will need to be split up into parts
    // that show where this number comes from
    const unsigned global_fd_estimate_ = 30;
    return global_fd_estimate_ + backendFDEstimate();
}


uint64_t
VolManager::volumeFDEstimate(const unsigned num_vols) const
{
    // this should be split up to show it's constituents
    const unsigned volume_fd_estimate_ = 15;
    return (num_vols * (open_scos_per_volume.value()+ volume_fd_estimate_));
}

uint64_t
VolManager::volumePotentialOpenFileDescriptors()
{
    const uint64_t max_file_descriptors = getMaxFileDescriptors();

    const unsigned num_volumes = volMap_.size() + restartMap_.size();

    const uint64_t num_fds_used = globalFDEstimate() +
        volumeFDEstimate(num_volumes);


    const uint64_t number_of_fds_left = (max_file_descriptors >= num_fds_used) ?
        (max_file_descriptors - num_fds_used) : 0 ;


    return number_of_fds_left / volumeFDEstimate();

}

uint64_t
VolManager::volumePotential(const ClusterSize c,
                            const SCOMultiplier s,
                            const boost::optional<TLogMultiplier>& t)
{
    const uint64_t fdpot = volumePotentialOpenFileDescriptors();

    LOCK_MANAGER();

    return std::min(fdpot,
                    volumePotentialSCOCache(c,
                                            s,
                                            t));
}

uint64_t
VolManager::volumePotential(const be::Namespace& nspace)
{
    const VolumeConfig cfg(VolumeConfigPersistor::load(*createBackendInterface(nspace)));
    return volumePotential(ClusterSize(cfg.getClusterSize()),
                           cfg.sco_mult_,
                           cfg.tlog_mult_);
}

void
VolManager::checkSCOAndTLogMultipliers(const ClusterSize csize,
                                       const SCOMultiplier sco_mult_old,
                                       const SCOMultiplier sco_mult_new,
                                       const boost::optional<TLogMultiplier>& tlog_mult_old,
                                       const boost::optional<TLogMultiplier>& tlog_mult_new)
{
    auto fun([&](const SCOMultiplier sm,
                 const boost::optional<TLogMultiplier>& t) -> uint64_t
             {
                 const uint64_t tm = t ? *t : number_of_scos_in_tlog.value();
                 return sm * tm * csize;
             });

    const uint64_t sco_cache_size_old = fun(sco_mult_old,
                                            tlog_mult_old);
    const uint64_t sco_cache_size_new = fun(sco_mult_new,
                                            tlog_mult_new);
    const int64_t diff = sco_cache_size_new - sco_cache_size_old;

    if (diff > 0)
    {
        const int64_t sco_cache_capacity = getSCOCacheCapacityWithoutThrottling();
        const int64_t sco_cache_usage = getCurrentVolumesTLogRequirements();

        if (sco_cache_capacity - sco_cache_usage < diff)
        {
            LOG_ERROR("Cannot change params " <<
                      sco_mult_old << " -> " << sco_mult_new << ", " <<
                      tlog_mult_old << " -> " << tlog_mult_new <<
                      " - insufficient SCO cache space: capacity " <<
                      sco_cache_capacity <<
                      ", current limit " << sco_cache_size_old <<
                      ", want " << sco_cache_size_new <<
                      ", diff " << diff);
            throw InsufficientResourcesException("SCOCache would be overcommitted");
        }
    }
}

void
VolManager::ensureResourceLimits(const VolumeConfig& config)
{
    const VolumeId id(config.id_);

    LOG_TRACE("Volume " << id);

    mgmtMutex_.assertLocked();

    if (volumePotentialOpenFileDescriptors() == 0)
    {
        LOG_ERROR(id << ": open file limit would be exceeded");
        throw InsufficientResourcesException("Open file limit would be exceeded",
                                             id.str().c_str(),
                                             ENFILE);
    }


    // Cache size check
    // const uint32_t scosize = ;
    // uint64_t requiredCacheSize = (scosize * number_of_scos_in_tlog.value())
    //     + getCurrentVolumesTLogRequirements();

    //TODO [BDV] set back to read from config and give ss-mgmt an atomic reference
    // uint64_t requiredCacheSize = number_of_scos_in_tlog.value() * scoSizeSum;
    // uint64_t availableCacheSize = getSCOCacheCapacityWithoutThrottling();


    if(volumePotentialSCOCache(ClusterSize(config.getClusterSize()),
                               config.sco_mult_,
                               config.tlog_mult_) == 0)
    {
        LOG_ERROR("SCO cache size is not large enough to create volume");
        throw InsufficientResourcesException("cache size is not large enough");
    }

    if(not checkEnoughFreeSpace_(metadata_path.value(),
                                 required_meta_freespace.value()))
    {
        LOG_ERROR(id << ": not enough free space on metadata partition " <<
                  metadata_path.value() << " to start a new volume");
        throw InsufficientResourcesException("Not enough free space on metadata partition",
                                             id.str().c_str());
    }
}

void
VolManager::ensure_metadata_freespace_meticulously_(const VolumeConfig& cfg)
{
    mgmtMutex_.assertLocked();

    uint64_t required = 0;
    uint64_t used = 0;

    for (const auto& v : restartMap_)
    {
        const VolumeConfig& c = v.second;
        required += VolumeFactory::metadata_locally_required_bytes(c);
        used += VolumeFactory::metadata_locally_used_bytes(c);
    }

    const uint64_t tba = required + VolumeFactory::metadata_locally_required_bytes(cfg) - used;

    if (not check_enough_free_space_(metadata_path.value(),
                                     tba + required_meta_freespace.value(),
                                     tba + required_meta_freespace.value() * 2))
    {
        LOG_ERROR("not enough free space on metadata partition " <<
                  metadata_path.value() << " to restart or clone " << cfg.id_);
        throw InsufficientResourcesException("Not enough free space on metadata partition");
    }
}

void
VolManager::setNamespaceRestarting_(const Namespace& ns, const VolumeConfig& config)
{
    mgmtMutex_.assertLocked();

    const auto res(restartMap_.insert(RestartMap::value_type(ns, config)));
    if (not res.second)
    {
        throw fungi::IOException("Failed to insert namespace into restart map",
                                 ns.str().c_str());
    }
}

template<typename V>
V
VolManager::with_restart_map_and_unlocked_mgmt_(const std::function<V(const Namespace& ns,
                                                                      const VolumeConfig& cfg)>& fun,
                                                const Namespace& ns,
                                                const VolumeConfig& cfg)
{
    mgmtMutex_.assertLocked();

    setNamespaceRestarting_(ns, cfg);

    BOOST_SCOPE_EXIT_TPL((&mgmtMutex_)(&restartMap_)(&ns))
    {
        mgmtMutex_.assertLocked();
        restartMap_.erase(ns);
    }
    BOOST_SCOPE_EXIT_END;

    UNLOCK_MANAGER();
    return fun(ns, cfg);
}

SharedVolumePtr
VolManager::with_restart_map_and_unlocked_mgmt_vol_(const std::function<SharedVolumePtr(const Namespace& ns,
                                                                                        const VolumeConfig& cfg)>& fun,
                                                    const Namespace& ns,
                                                    const VolumeConfig& cfg)
{
    SharedVolumePtr vol = with_restart_map_and_unlocked_mgmt_<SharedVolumePtr>(fun, ns, cfg);

    mgmtMutex_.assertLocked();

    ASSERT(vol);

    volMap_[vol->getName()] = vol;

    return vol;
}

SharedVolumePtr
VolManager::createClone(const CloneVolumeConfigParameters& clone_params,
                        const PrefetchVolumeData prefetch,
                        const CreateNamespace create_namespace)
{
    mgmtMutex_.assertLocked();

    // we might need to set the parent snapshot
    CloneVolumeConfigParameters params(clone_params);

    VERIFY(params.get_parent_nspace());

    const VolumeId id(params.get_volume_id());
    const be::Namespace parent_ns(*params.get_parent_nspace());
    const be::Namespace ns(params.get_nspace());

    MAIN_EVENT("Volume Clone, VolumeId: " << id
               << ", Namespace:" << ns
               << ", Parent Namespace " << parent_ns
               << ", Parent Snapshot " << params.get_parent_snapshot());

    ensureVolumeNotPresent(id);
    ensureVolumeNotPresent(params.get_nspace());
    ensureNamespace(parent_ns,
                    CreateNamespace::F);
    ensureNamespace(ns,
                    create_namespace);

    auto on_exit(yt::make_scope_exit_on_exception([&]
                                                  {
                                                      if (create_namespace == CreateNamespace::T)
                                                      {
                                                          remove_namespace_(ns);
                                                      }
                                                  }));

    const std::unique_ptr<SnapshotPersistor>
        sp(SnapshotManagement::createSnapshotPersistor(createBackendInterface(parent_ns)));

    std::unique_ptr<VolumeConfig> volume_config;

    const VolumeConfig cfg_old(VolumeConfigPersistor::load(*createBackendInterface(parent_ns)));

    if (cfg_old.wan_backup_volume_role_ ==
        VolumeConfig::WanBackupVolumeRole::WanBackupIncremental)
    {
        LOG_ERROR("Namespace " << ns <<
                  ", volume " << id <<
                  ": cannot create a clone from namespace " << parent_ns <<
                  ", snapshot " << params.get_parent_snapshot() <<
                  " as it has VolumeRole::Incremental");
        throw VolumeDoesNotHaveCorrectRole("No clones from incremental volumes");
    }

    // A bit hackish, but I don't want to boost optional for the parent_snapshot it VolumeConfig now.

    if(not params.get_parent_snapshot())
    {
        if(F(cfg_old.isVolumeTemplate()))
        {
            LOG_ERROR("No snapshot specified, but volume is not a template volume");
            throw VolumeNotTemplatedButNoSnapshotSpecifiedException("No snapshot specified, but volume is not a template volume");

        }
        std::vector<SnapshotNum> snaps;
        sp->getAllSnapshots(snaps);
        VERIFY(snaps.size() == 1);
        params.parent_snapshot(sp->getSnapshotName(snaps.front()));
    }

    VERIFY(params.get_parent_snapshot());

    if (not sp->isSnapshotInBackend(sp->getSnapshotNum(*params.get_parent_snapshot())))
    {
        LOG_ERROR("Snapshot " << *params.get_parent_snapshot() << " is not in backend");
        throw SnapshotNotOnBackendException("Snapshot not in backend");
    }

    if (not params.get_cluster_cache_mode())
    {
        params.cluster_cache_mode(cfg_old.cluster_cache_mode_);
    }
    else if (*params.get_cluster_cache_mode() == ClusterCacheMode::ContentBased)
    {
        TODO("AR: I don't like this being here and a similar one in Volume::setClusterCacheMode - unify!");
        if (cfg_old.cluster_cache_mode_ and
            cfg_old.cluster_cache_mode_ == ClusterCacheMode::LocationBased)
        {
            LOG_ERROR(id << ": setting the cluster cache mode of a clone to " <<
                      ClusterCacheMode::ContentBased <<
                      " while the parent's " << cfg_old.id_ << " mode is " <<
                      ClusterCacheMode::LocationBased <<
                      " is not supported");
            throw fungi::IOException("Changing the cluster cache mode from LocationBased (parent) to ContentBased (clone) is not supported");
        }
    }

    const VolumeConfig config(params,
                              cfg_old);

    const youtils::UUID
        parent_snap_uuid(sp->getSnapshotCork(*params.get_parent_snapshot()));

    ensureResourceLimits(config);

    ensure_metadata_freespace_meticulously_(config);

    auto fun([&params,
              &prefetch,
              &parent_snap_uuid](const Namespace& /* ns */,
                                 const VolumeConfig& cfg) -> SharedVolumePtr
             {
                 return VolumeFactory::createClone(cfg,
                                                   prefetch,
                                                   parent_snap_uuid);

                 LOG_NOTIFY("Volume Clone complete, VolumeId: " << params.get_volume_id());
             });

    return with_restart_map_and_unlocked_mgmt_vol_(fun,
                                                   params.get_nspace(),
                                                   config);
}

void
VolManager::getClusterCacheStats(uint64_t& num_hits,
                                 uint64_t& num_misses,
                                 uint64_t& num_entries)
{
    ClusterCache_.get_stats(num_hits,
                            num_misses,
                            num_entries);
}

VolumeSize
VolManager::real_max_volume_size() const
{
    const auto ret = VolumeSize(max_volume_size.value() + barts_correction);
    VERIFY(ret <= Entry::max_valid_cluster_address() *
           VolumeConfig::default_cluster_size());
    return ret;
}

void
VolManager::ensure_volume_size_(VolumeSize size) const
{
    if (size > real_max_volume_size())
    {
        const uint64_t max_size = max_volume_size.value();

        LOG_ERROR("Cannot create a volume with size " << size <<
                  ", only supported up to " << yt::DimensionedValue(max_size) <<
                  " (" << max_size << " bytes)");
        throw fungi::IOException("Requested volume size too large");
    }
}

WriteOnlyVolume*
VolManager::createNewWriteOnlyVolume(const WriteOnlyVolumeConfigParameters& params)
{
    mgmtMutex_.assertLocked();

    const VolumeId id(params.get_volume_id());
    const be::Namespace ns(params.get_nspace());
    const VolumeSize size(params.get_size());

    MAIN_EVENT(" new WriteOnlyVolume, VolumeId: " << id
               << ", Namespace: " << ns
               << ", Size: " << size);

    ensureVolumeNotPresent(id);
    ensureVolumeNotPresent(ns);
    ensureNamespace(ns,
                    CreateNamespace::F);

    ensure_volume_size_(size);

    const VolumeConfig cfg(params);
    ensureResourceLimits(cfg);

    TODO("AR: use unique_ptr instead of raw one");
    WriteOnlyVolume* vol = VolumeFactory::createWriteOnlyVolume(cfg).release();
    ASSERT(vol);

    return vol;
}

SharedVolumePtr
VolManager::createNewVolume(const VanillaVolumeConfigParameters& params,
                            const CreateNamespace create_namespace)
{
    mgmtMutex_.assertLocked();

    const VolumeId id(params.get_volume_id());
    const be::Namespace ns(params.get_nspace());
    const VolumeSize size(params.get_size());

    MAIN_EVENT("New Volume, VolumeId: " << id
               << ", Namespace: " << ns
               << ", Size: " << size
               << ", CreateNamespace: " << create_namespace);

    ensureVolumeNotPresent(id);
    ensureVolumeNotPresent(ns);
    ensureNamespace(ns,
                    create_namespace);

    auto on_exit(yt::make_scope_exit_on_exception([&]
                                                  {
                                                      if (create_namespace == CreateNamespace::T)
                                                      {
                                                          remove_namespace_(ns);
                                                      }
                                                  }));

    ensure_volume_size_(size);

    const VolumeConfig cfg(params);
    ensureResourceLimits(cfg);

    auto fun([this](const Namespace&,
                    const VolumeConfig& cfg) -> SharedVolumePtr
             {
                 SharedVolumePtr vol(VolumeFactory::createNewVolume(cfg));
                 ASSERT(vol);
                 return vol;
             });

    return with_restart_map_and_unlocked_mgmt_vol_(fun,
                                                   params.get_nspace(),
                                                   cfg);
}

void
VolManager::remove_namespace_(const Namespace& ns) noexcept
{
    try
    {
        createBackendInterface(ns)->deleteNamespace();
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to remove namespace " << ns);
}

void
VolManager::ensureNamespace(const Namespace& ns,
                            const CreateNamespace create_namespace)
{
     //LOG_INFO("ensuring namespace " << ns.str());
    if(T(create_namespace))
    {
        BackendInterfacePtr bi(createBackendInterface(ns));
        VERIFY(not bi->namespaceExists());
        // OVS-2996: use NamespaceMustNotExist::F to work around a retry race during
        // namespace creation with ALBA.
        bi->createNamespace(NamespaceMustNotExist::F);
    }
    else if (not createBackendInterface(ns)->namespaceExists())
    {
        throw NamespaceDoesNotExistException("namespace doesn't exist");
    }
}

void
VolManager::restoreSnapshot(const VolumeId& volid,
                            const SnapshotName& snapid)
{
    mgmtMutex_.assertLocked();

    MAIN_EVENT("Restoring Snapshot, VolumeID: " << volid << ", SnapshotName: " << snapid);

    SharedVolumePtr vol = find_volume(volid);
    const VolumeConfig cfg(vol->get_config());
    const be::Namespace nspace(vol->getNamespace());

    volMap_.erase(volid);

    auto fun([&](const be::Namespace&,
                 const VolumeConfig&) -> SharedVolumePtr
             {
                 vol->restoreSnapshot(snapid);
                 return vol;
             });

    auto self = this;

    BOOST_SCOPE_EXIT((&self)(&vol))
    {
        self->volMap_[vol->getName()] = vol;
    }
    BOOST_SCOPE_EXIT_END;

    with_restart_map_and_unlocked_mgmt_<SharedVolumePtr>(fun, nspace, cfg);
}

SharedVolumePtr
VolManager::local_restart(const Namespace& ns,
                          const OwnerTag owner_tag,
                          const FallBackToBackendRestart fallback,
                          const IgnoreFOCIfUnreachable ignoreFOCIfUnreachable)
{
    mgmtMutex_.assertLocked();
    MAIN_EVENT("Local Restart, Namespace " << ns << ", owner tag " << owner_tag);
    ensureVolumeNotPresent(ns);

    const VolumeConfig config(VolumeConfigPersistor::load(*createBackendInterface(ns)));
    ensureVolumeNotPresent(config.id_);

    THROW_WHEN(fallback == FallBackToBackendRestart::T and
               config.wan_backup_volume_role_ !=
               VolumeConfig::WanBackupVolumeRole::WanBackupNormal);

    ensureResourceLimits(config);
    ensure_metadata_freespace_meticulously_(config);

    auto fun([fallback,
              ignoreFOCIfUnreachable,
              owner_tag](const Namespace&,
                         const VolumeConfig& cfg) -> SharedVolumePtr
             {
                 return VolumeFactory::local_restart(cfg,
                                                     owner_tag,
                                                     fallback,
                                                     ignoreFOCIfUnreachable);
             });

    return with_restart_map_and_unlocked_mgmt_vol_(fun,
                                                   ns,
                                                   config);
}

void
VolManager::setAsTemplate(const VolumeId volid)
{
    mgmtMutex_.assertLocked();
    MAIN_EVENT("Set Volume As Template, volume ID " << volid);
    SharedVolumePtr vol = find_volume(volid);
    {
        UNLOCK_MANAGER();
        vol->setAsTemplate();
    }
}

SharedVolumePtr
VolManager::backend_restart(const Namespace& ns,
                            const OwnerTag owner_tag,
                            const PrefetchVolumeData prefetch,
                            const IgnoreFOCIfUnreachable ignore_foc)
{
    mgmtMutex_.assertLocked();
    MAIN_EVENT("Backend Restart, Namespace: " << ns << ", owner tag " << owner_tag);

    ensureVolumeNotPresent(ns);

    // We could have stale date in some local cache (I'm looking at you, Alba) if
    // the volume was created here and then moved (and used) elsewhere. Better be
    // safe than sorry. The SCOCache needs the same treatment at a later stage
    // (cf. VolumeFactory::backend_restart).
    createBackendInterface(ns)->invalidate_cache();

    //to get info about scosize already on restarting volumes we need to get volconfig here
    const VolumeConfig config(VolumeConfigPersistor::load(*createBackendInterface(ns)));

    ensureVolumeNotPresent(config.id_);

    if(config.wan_backup_volume_role_ !=
       VolumeConfig::WanBackupVolumeRole::WanBackupNormal)
    {
        LOG_ERROR("Trying to restart " << ns <<
                  " but the volume there doesn't have VolumeRole::Normal");
        throw VolumeDoesNotHaveCorrectRole("Volume does not have normal role, you are restarting a backup");
    }

    ensureResourceLimits(config);
    ensure_metadata_freespace_meticulously_(config);

    auto fun([prefetch,
              ignore_foc,
              owner_tag](const Namespace&, const VolumeConfig& cfg) -> SharedVolumePtr
             {
                 return VolumeFactory::backend_restart(cfg,
                                                       owner_tag,
                                                       prefetch,
                                                       ignore_foc);
             });

    return with_restart_map_and_unlocked_mgmt_vol_(fun, ns, config);
}

WriteOnlyVolume*
VolManager::restartWriteOnlyVolume(const Namespace& ns,
                                   const OwnerTag owner_tag)
{
    mgmtMutex_.assertLocked();
    MAIN_EVENT("Restart WriteOnlyVolume, Namespace: " << ns << ", owner tag " <<
               owner_tag);

    ensureVolumeNotPresent(ns);

    //to get info about scosize already on restarting volumes we need to get volconfig here
    const VolumeConfig volume_config(VolumeConfigPersistor::load(*createBackendInterface(ns)));

    // If this is removed you might want to do explicit checking in Backup.cpp for volume role
    if(volume_config.wan_backup_volume_role_ ==
       VolumeConfig::WanBackupVolumeRole::WanBackupNormal)
    {
        LOG_ERROR("Trying to restart " << ns << " as WriteOnlyVolume the volume there doesn't have VolumeRole::Normal");
        throw VolumeDoesNotHaveCorrectRole("Volume does not have Backup role, you are restarting a normal volume as readonly");
    }

    ensureResourceLimits(volume_config);

    auto fun([owner_tag](const Namespace&,
                         const VolumeConfig& cfg) -> WriteOnlyVolume*
             {
                 return VolumeFactory::backend_restart_write_only_volume(cfg,
                                                                         owner_tag).release();
             });

    return with_restart_map_and_unlocked_mgmt_<WriteOnlyVolume*>(fun, ns, volume_config);
}

void
VolManager::destroyVolume(const VolumeId& name,
                          const DeleteLocalData delete_local_data,
                          const RemoveVolumeCompletely remove_volume_completely,
                          const DeleteVolumeNamespace delete_volume_namespace,
                          const ForceVolumeDeletion force_volume_deletion)
{
    mgmtMutex_.assertLocked();

    SharedVolumePtr v = find_volume(name);
    destroyVolume(v,
                  delete_local_data,
                  remove_volume_completely,
                  delete_volume_namespace,
                  force_volume_deletion);
}

void
VolManager::destroyVolume(SharedVolumePtr v,
                          const DeleteLocalData delete_local_data,
                          const RemoveVolumeCompletely remove_volume_completely,
                          const DeleteVolumeNamespace delete_volume_namespace,
                          const ForceVolumeDeletion force_volume_deletion)
{
    mgmtMutex_.assertLocked();

    if(T(remove_volume_completely) and
       F(delete_local_data))
    {
        LOG_ERROR("Incompatible arguments: remove_volume_completely::T and delete_local_data::F" <<
                  ", retry with correct arguments");
        throw fungi::IOException("Incompatible arguments: remove_volume_completely::T and delete_local_data::F");
    }

    MAIN_EVENT("Destroy Volume, VolumeId: " << v->getName() <<
               ", delete local data: " << delete_local_data <<
               ", remove volume completely " << remove_volume_completely <<
               ", delete namespace " << delete_volume_namespace <<
               ", force deletion " << force_volume_deletion);

    volMap_.erase(v->getName());

    // We only trigger deletion here when the previous stuff did not throw.
    // http://jira.openvstorage.com/browse/OVS-827
    bool proceed = false;
    auto on_exit(yt::make_scope_exit([&]
                                     {
                                         if (not proceed)
                                         {
                                             volMap_.insert(std::make_pair(v->getName(),
                                                                           v));
                                         }
                                     }));

    auto fun([&](const be::Namespace&,
                 const VolumeConfig&) -> SharedVolumePtr
             {
                 v->destroy(delete_local_data,
                            remove_volume_completely,
                            force_volume_deletion);
                 return v;
             });

    with_restart_map_and_unlocked_mgmt_<SharedVolumePtr>(fun,
                                                         v->getNamespace(),
                                                         v->get_config());

    proceed = true;

    if(T(delete_volume_namespace))
    {
        remove_namespace_(v->getNamespace());
    }
}

void
VolManager::destroyVolume(WriteOnlyVolume *v,
                          const RemoveVolumeCompletely remove_completely)
{
    mgmtMutex_.assertLocked();
    MAIN_EVENT("Destroy WriteOnlyVolume, VolumeId: " << v->getName());

    v->destroy(remove_completely);
    delete v;
}

void
VolManager::removeLocalVolumeData(const be::Namespace& nspace)
{
    ASSERT_LOCKABLE_LOCKED(mgmtMutex_);

    SharedVolumePtr vol = find_volume_no_throw(nspace);
    if (vol != nullptr)
    {
        LOG_ERROR("Volume " << vol->getName() << " backed by namespace " <<
                  vol->getNamespace() << " is present - cannot remove local data");
        throw fungi::IOException("Cannot remove local volume data while volume is present",
                                 nspace.str().c_str());
    }

    ensureNamespaceNotRestarting(nspace);

    fs::remove_all(getMetaDataPath(nspace));
    fs::remove_all(getTLogPath(nspace));

    try
    {
        scoCache_.removeDisabledNamespace(nspace);
    }
    CATCH_STD_ALL_LOG_IGNORE("Failed to remove namespace " << nspace <<
                             " from SCOCache");
}

void
VolManager::getVolumesOverview(std::map<VolumeId, VolumeOverview>& out_map) const
{
    mgmtMutex_.assertLocked();
    for (VolumeMap::const_iterator it = volMap_.begin();
         it != volMap_.end();
         ++it)
    {
        out_map.insert(std::make_pair(it->first,
                                      VolumeOverview(it->second->getNamespace(),
                                                     VolumeOverview::VolumeState::Active)));


    }
    for(RestartMap::const_iterator it = restartMap_.begin();
        it != restartMap_.end();
        ++it)
    {
        out_map.insert(std::make_pair(it->second.id_,
                                      VolumeOverview(it->first,
                                                     VolumeOverview::VolumeState::Restarting)));
    }
}


void
VolManager::getVolumeList(std::list<VolumeId> &vlist) const
{
    mgmtMutex_.assertLocked();
    for (VolumeMap::const_iterator it = volMap_.begin();
         it != volMap_.end();
         ++it)
    {
        vlist.push_back(it->first);
    }
}

SharedVolumePtr
VolManager::find_volume_no_throw(const VolumeId& id)  const
{
    mgmtMutex_.assertLocked();

    auto it = volMap_.find(id);
    if (it == volMap_.end())
    {
        return nullptr;
    }
    else
    {
        return it->second;
    }
}

SharedVolumePtr
VolManager::find_volume_no_throw(const Namespace& nspace) const
{
    mgmtMutex_.assertLocked();

    for (const auto& p : volMap_)
    {
        if (p.second->getNamespace() == nspace)
        {
            return p.second;
        }
    }

    return nullptr;
}

VolPool*
VolManager::backend_thread_pool()
{
    return &backend_thread_pool_;
}

fungi::Mutex&
VolManager::getLock_()
{
    return mgmtMutex_;
}

template<typename T>
class ThreadPoolCounter
{
public:

    ThreadPoolCounter()
        :result(0)
    {}
    void operator()(const T* )
    {
        ++result;
    }

    uint64_t
    getResult() const
    {
        return result;
    }

    typedef T type;

private:
    uint64_t result;

};

uint64_t
VolManager::getQueueCount(const VolumeId& volName)
{
    SharedVolumePtr v = find_volume(volName);

    try
    {
        ThreadPoolCounter<backend_task::WriteSCO> counter;
        backend_thread_pool_.mapper(v.get(),
                                    counter);
        return counter.getResult();
    }
    catch(...)
    {
        throw;
    }

}

uint64_t
VolManager::getQueueSize(const VolumeId& volName)
{
    //returns an estimate of data
    SharedVolumePtr v = find_volume(volName);
    return v->getSCOSize() * getQueueCount(volName);
}

bool
VolManager::updateReadActivity()
{
    LOG_PERIODIC("Updating read activity");
    mgmtMutex_.assertLocked();
    time_t current_time = time(0);

    if (current_time == -1)
    {
        LOG_ERROR("Could not update read activities, current time not available");
        return false;
    }

    if (read_activity_time_ == -1)
    {
        read_activity_time_ = current_time;
        LOG_ERROR("Could not update read activities, previous time not available");
        return false;
    }

    if (current_time <= read_activity_time_)
    {
        read_activity_time_ = current_time;
        LOG_WARN("Could not update read activities, current time smaller or equal to previous time");
        return false;
    }

    time_t diff = current_time - read_activity_time_;

    for (VolumeMap::iterator it = volMap_.begin(); it != volMap_.end(); ++it)
    {
        try
        {
            it->second->updateReadActivity(diff);
        }
        catch (...)
        {
            LOG_ERROR("Problem updateing read activity for volume " << it->first);
        }
    }

    read_activity_time_ = current_time;
    return true;
}

BackendInterfacePtr
VolManager::createBackendInterface(const Namespace& ns)
{
    VERIFY(backend_conn_manager_ != nullptr);
    return backend_conn_manager_->newBackendInterface(ns);
}

double
VolManager::readActivity()
{
    double res = 0;
    for(VolumeMap::const_iterator it = volMap_.begin();
        it != volMap_.end();
        ++it)
    {
        VERIFY(it->second);

        double ra = it->second->readActivity();
        VERIFY(ra >= 0);
        res += ra;
    }
    return res;
}

void
VolManager::getClusterCacheDeviceInfo(ClusterCache::ManagerType::Info& info)
{
    ClusterCache_.deviceInfo(info);
}



bool
VolManager::checkEnoughFreeSpace_(const fs::path& p,
                                  uint64_t minimal) const
{
    return check_enough_free_space_(p, minimal, 2 * minimal);
}

bool
VolManager::check_enough_free_space_(const fs::path& p,
                                     uint64_t minimal,
                                     uint64_t warn_threshold) const
{
    VERIFY(warn_threshold >= minimal);

    const uint64_t fri = FileUtils::filesystem_free_size(p);

    if (fri < minimal)
    {
        LOG_ERROR("available space on " << p << " insufficient: " << fri <<
                  " < " << minimal << " (minimally required)");
        VolumeDriverError::report(events::VolumeDriverErrorCode::DiskSpace,
                                  p.string());
    }
    else if (fri < warn_threshold)
    {
        LOG_WARN("available space on " << p << " getting on the low side: " << fri <<
                 " < " << warn_threshold << " (warning threshold, " << minimal <<
                 " required minimally)");
        VolumeDriverError::report(events::VolumeDriverErrorCode::DiskSpace,
                                  p.string());
    }
    else
    {
        LOG_PERIODIC("checked available space on " << p << ", OK. (" << fri << " > " << minimal << ")");
    }

    return fri >= minimal;
}

void
VolManager::checkMetaDataFreeSpace()
{
    //no locking is necessary

    bool metaOK( checkEnoughFreeSpace_(metadata_path.value(),
                                       required_meta_freespace.value()));
    bool tlogsOK(checkEnoughFreeSpace_(tlog_path.value(),
                                       required_tlog_freespace.value()));
    bool ok = metaOK and tlogsOK;

    if (ok and readOnlyMode_)
    {
        LOG_PERIODIC("Switching to read/write mode again for all volumes");
    }
    if((not ok) and (not readOnlyMode_))
    {
        LOG_ERROR("Switching (temporarily) to readonly mode for all volumes");
    }

    readOnlyMode_ = not ok;
}

void
VolManager::checkVolumeFailoverCaches()
{
    std::vector<VolumeId> vols;

    // TODO: technically a copy of the map should also work which could then
    // be used without the mgmt mutex held, but this needs thorough review /
    // checking of e.g. volume removal vs. this one.
    // Play it safe for now.
    {
        LOCK_MANAGER();
        vols.reserve(volMap_.size());

        for(const auto& p : volMap_)
        {
            vols.push_back(p.first);
        }
    }

    for (const auto& vol : vols)
    {
        LOCK_MANAGER();
        SharedVolumePtr v(find_volume_no_throw(vol));
        if (v)
        {
            v->check_and_fix_failovercache();
        }
    }
}

uint64_t
VolManager::get_sco_cache_max_non_disposable_bytes(const VolumeConfig& cfg) const
{
    return ceilf(cfg.getSCOSize() *
                 (cfg.tlog_mult_ ? cfg.tlog_mult_.get() : number_of_scos_in_tlog.value()) *
                 (cfg.max_non_disposable_factor_ ? cfg.max_non_disposable_factor_.get() : non_disposable_scos_factor.value()));
}

bool
VolManager::checkConfig(const boost::property_tree::ptree& pt,
                        ConfigurationReport& rep) const
{
    bool result = true;

    {
        PARAMETER_TYPE(open_scos_per_volume) val(pt);
        if(val.value()  == 0 or val.value() >= 1024)
        {
            std::stringstream ss;
            ss << "Value " << val.value() <<
                " for open scos per volume should be between 0 and 1024";

            ConfigurationProblem s(PARAMETER_TYPE(open_scos_per_volume)::name(),
                                   PARAMETER_TYPE(open_scos_per_volume)::section_name(),
                                   ss.str());
            rep.push_front(s);
            result = false;
        }
    }

    {
        PARAMETER_TYPE(number_of_scos_in_tlog) val(pt);
        if (val.value() < 1)
        {
            rep.push_front(ConfigurationProblem(val.name(),
                                                val.section_name(),
                                                "number_of_scos_in_tlog must be >= 1"));
            result = false;
        }
    }

    {
        PARAMETER_TYPE(non_disposable_scos_factor) val(pt);
        if (val.value() < 1.0)
        {
            rep.push_front(ConfigurationProblem(val.name(),
                                                val.section_name(),
                                                "non_disposable_scos_factor must be >= 1"));
            result = false;
        }
    }

    {
        PARAMETER_TYPE(max_volume_size) val(pt);
        if (val.value() + barts_correction >
            Entry::max_valid_cluster_address() * VolumeConfig::default_cluster_size())
        {
            rep.push_front(ConfigurationProblem(val.name(),
                                                val.section_name(),
                                                "max_volume_size is limited by the Entry::max_valid_cluster_address"));
            result = false;
        }
    }

    {
        PARAMETER_TYPE(read_cache_default_behaviour) val(pt);
        if (val.value() != read_cache_default_behaviour.value())
        {
            rep.push_front(ConfigurationProblem(val.name(),
                                                val.section_name(),
                                                "read_cache_default_behaviour cannot be dynamically reconfigured"));
            result = false;
        }
    }

    {
        PARAMETER_TYPE(read_cache_default_mode) val(pt);
        if (val.value() != read_cache_default_mode.value())
        {
            rep.push_front(ConfigurationProblem(val.name(),
                                                val.section_name(),
                                                "read_cache_default_mode cannot be dynamically reconfigured"));
            result = false;
        }
    }

    return result;
}

const BackendConfig&
VolManager::getBackendConfig() const
{
    return backend_conn_manager_->config();
}

// VolumeDriverComponent Interface
void
VolManager::update(const boost::property_tree::ptree& pt,
                   UpdateReport& report)
{
    tlog_path.update(pt, report);
    metadata_path.update(pt, report);
    open_scos_per_volume.update(pt, report);
    dtl_throttle_usecs.update(pt, report);
    dtl_queue_depth.update(pt, report);
    dtl_write_trigger.update(pt, report);
    dtl_busy_loop_usecs.update(pt, report);
    dtl_request_timeout_ms.update(pt, report);
    dtl_connect_timeout_ms.update(pt, report);

    freespace_check_interval.update(pt, report);
    dtl_check_interval_in_seconds.update(pt, report);

    read_cache_default_behaviour.update(pt, report);
    read_cache_default_mode.update(pt, report);
    sco_written_to_backend_action.update(pt, report);
    clean_interval.update(pt, report);
    sap_persist_interval.update(pt, report);
    required_meta_freespace.update(pt, report);
    required_tlog_freespace.update(pt, report);
    max_volume_size.update(pt, report);
    number_of_scos_in_tlog.update(pt, report);
    non_disposable_scos_factor.update(pt, report);
    default_cluster_size.update(pt, report);
    metadata_cache_capacity.update(pt, report);
    metadata_mds_slave_max_tlogs_behind.update(pt, report);
    debug_metadata_path.update(pt, report);
    arakoon_metadata_sequence_size.update(pt, report);
    allow_inconsistent_partial_reads.update(pt, report);
    volume_nullio.update(pt, report);
}

void
VolManager::persist(boost::property_tree::ptree& pt,
                    const ReportDefault reportDefault) const
{
    metadata_path.persist(pt, reportDefault);
    tlog_path.persist(pt, reportDefault);
    open_scos_per_volume.persist(pt, reportDefault);
    freespace_check_interval.persist(pt, reportDefault);
    dtl_check_interval_in_seconds.persist(pt, reportDefault);
    read_cache_default_behaviour.persist(pt, reportDefault);
    read_cache_default_mode.persist(pt, reportDefault);
    sco_written_to_backend_action.persist(pt, reportDefault);
    clean_interval.persist(pt, reportDefault);
    sap_persist_interval.persist(pt, reportDefault);
    required_tlog_freespace.persist(pt, reportDefault);
    required_meta_freespace.persist(pt, reportDefault);
    max_volume_size.persist(pt, reportDefault);
    dtl_throttle_usecs.persist(pt, reportDefault);
    dtl_queue_depth.persist(pt, reportDefault);
    dtl_write_trigger.persist(pt, reportDefault);
    dtl_busy_loop_usecs.persist(pt, reportDefault);
    dtl_request_timeout_ms.persist(pt, reportDefault);
    dtl_connect_timeout_ms.persist(pt, reportDefault);

    number_of_scos_in_tlog.persist(pt, reportDefault);
    non_disposable_scos_factor.persist(pt, reportDefault);
    default_cluster_size.persist(pt, reportDefault);
    metadata_cache_capacity.persist(pt, reportDefault);
    metadata_mds_slave_max_tlogs_behind.persist(pt, reportDefault);
    debug_metadata_path.persist(pt, reportDefault);
    arakoon_metadata_sequence_size.persist(pt, reportDefault);
    allow_inconsistent_partial_reads.persist(pt, reportDefault);
    volume_nullio.persist(pt, reportDefault);
}

std::shared_ptr<metadata_server::Manager>
VolManager::metadata_server_manager()
{
    return mds_manager_;
}

ClusterCacheBehaviour
VolManager::get_cluster_cache_default_behaviour() const
{
    return read_cache_default_behaviour.value();
}

ClusterCacheMode
VolManager::get_cluster_cache_default_mode() const
{
    return read_cache_default_mode.value();
}

size_t
VolManager::effective_metadata_cache_capacity(const VolumeConfig& cfg) const
{
    return cfg.metadata_cache_capacity_ ?
        *cfg.metadata_cache_capacity_ :
        metadata_cache_capacity.value();
}

SCOWrittenToBackendAction
VolManager::get_sco_written_to_backend_action() const
{
    return sco_written_to_backend_action.value();
}

boost::optional<uint32_t>
VolManager::mds_slave_max_tlogs_behind() const
{
    size_t v = metadata_mds_slave_max_tlogs_behind.value();
    if (v == std::numeric_limits<uint32_t>::max())
    {
        return boost::none;
    }
    else
    {
        return v;
    }
}

fs::path
VolManager::getTLogPath(const Volume& v) const
{
    return fs::path(tlog_path.value()) / v.getNamespace().str();
}

fs::path
VolManager::getMetaDataPath(const Volume& v) const
{
    return fs::path(metadata_path.value()) / v.getNamespace().str();
}

}

// Local Variables: **
// mode: c++ **
// End: **
