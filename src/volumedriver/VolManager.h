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

#ifndef VOLMANAGER_H_
#define VOLMANAGER_H_

#include "DataStoreCallBack.h"
#include "Events.h"
#include "SCOCache.h"
#include "SnapshotManagement.h"
#include "ClusterCache.h"
#include "Volume.h"
#include "VolumeConfigParameters.h"
#include "VolumeDriverParameters.h"
#include "VolumeDriverEvents.pb.h"
#include "VolumeFactory.h"
#include "VolumeOverview.h"
#include "VolumeThreadPool.h"
#include "WriteOnlyVolume.h"

#include <map>
#include <memory>
#include <string>

#include <boost/optional.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/variant.hpp>

#include <youtils/Notifier.h>
#include <youtils/PeriodicAction.h>
#include <youtils/VolumeDriverComponent.h>

#include <backend/BackendConfig.h>
//TODO [BDV] find out why forward decl doesn't work while following include does
#include <backend/BackendConnectionManager.h>
#include <backend/BackendInterface.h>
#include <backend/BackendPolicyConfig.h>

#include "failovercache/fungilib/Mutex.h"

class api;

namespace metadata_server
{

class Manager;

}

namespace volumedriver
{

class ClusterClusterCache;
class PeriodicAction;
class Volume;

namespace utils
{

class VolumeDriverComponent;

}

// this has singleton semantics
// TODO: the locking needs to be improved:
// methods accessing the volMap need to be serialized using the mgmtMutex_,
// however, the callchain does not necessarily make it obvious if the lock is
// actually held
class VolManager
    : public VolumeDriverComponent
{
public:
    typedef std::map<VolumeId, Volume* > VolumeMap;

    VolManager(const VolManager&) = delete;

    VolManager& operator=(const VolManager&) = delete;

    MAKE_EXCEPTION(VolManagerException, fungi::IOException);
    MAKE_EXCEPTION(VolumeDoesNotHaveCorrectRole, VolManagerException);
    MAKE_EXCEPTION(VolumeNameAlreadyPresent, VolManagerException);
    MAKE_EXCEPTION(NamespaceAlreadyPresent, VolManagerException);
    MAKE_EXCEPTION(VolumeDoesNotExistException, VolManagerException);
    MAKE_EXCEPTION(VolumeNotTemplatedButNoSnapshotSpecifiedException, VolManagerException);
    MAKE_EXCEPTION(InsufficientResourcesException, VolManagerException);
    MAKE_EXCEPTION(NamespaceDoesNotExistException, VolManagerException);

    //    friend class ::api;
    friend class VolumeTestSetup;
    friend class VolManagerTestSetup;
    friend class ::api;

    static
    const std::string&
    metaDataDBName()
    {
        static const std::string name_ = "mdstore.tc";
        return name_;
    }

    static void
    start(const fs::path&,
          events::PublisherPtr event_publisher = nullptr);

    static void
    start(const boost::property_tree::ptree&,
          events::PublisherPtr event_publisher = nullptr);

    static VolManager*
    get();

    static void
    stop();

    void
    restoreSnapshot(const VolumeId& volid,
                    const std::string& snapid);

    Volume*
    backend_restart(const Namespace& ns,
                    const OwnerTag,
                    const PrefetchVolumeData,
                    const IgnoreFOCIfUnreachable,
                    const uint32_t num_pages_cached = 1024);

    WriteOnlyVolume*
    restartWriteOnlyVolume(const Namespace& ns,
                           const OwnerTag);

    VolPool*
    backend_thread_pool();

    void
    scheduleTask(VolPoolTask* t);

    Volume*
    createNewVolume(const VanillaVolumeConfigParameters& params,
                    const CreateNamespace = CreateNamespace::F);

    WriteOnlyVolume*
    createNewWriteOnlyVolume(const WriteOnlyVolumeConfigParameters& params);

    Volume*
    createClone(const CloneVolumeConfigParameters& params,
                const PrefetchVolumeData prefetch,
                const CreateNamespace = CreateNamespace::F);

    void
    destroyVolume(const VolumeId&,
                  const DeleteLocalData = DeleteLocalData::F,
                  const RemoveVolumeCompletely = RemoveVolumeCompletely::F,
                  const DeleteVolumeNamespace = DeleteVolumeNamespace::F,
                  const ForceVolumeDeletion = ForceVolumeDeletion::F);

    void
    destroyVolume(Volume*,
                  const DeleteLocalData = DeleteLocalData::F,
                  const RemoveVolumeCompletely = RemoveVolumeCompletely::F,
                  const DeleteVolumeNamespace = DeleteVolumeNamespace::F,
                  const ForceVolumeDeletion = ForceVolumeDeletion::F);

    void
    destroyVolume(WriteOnlyVolume *,
                  const RemoveVolumeCompletely);

    void
    removeLocalVolumeData(const backend::Namespace& nspace);

    static constexpr double gamma = 1.0/3600.0;

    // Api for the outside
    bool
    updateConfiguration(const boost::property_tree::ptree&,
                        UpdateReport&,
                        ConfigurationReport&);

    boost::variant<youtils::UpdateReport,
                   youtils::ConfigurationReport>
    updateConfiguration(const boost::filesystem::path&);

    boost::variant<youtils::UpdateReport,
                   youtils::ConfigurationReport>
    updateConfiguration(const boost::property_tree::ptree&);

    bool
    checkConfiguration(const boost::property_tree::ptree&,
                       ConfigurationReport&);

    bool
    checkConfiguration(const boost::filesystem::path&,
                       ConfigurationReport&);

    void
    persistConfiguration(boost::property_tree::ptree&,
                         const ReportDefault = ReportDefault::F) const;

    void
    persistConfiguration(const boost::filesystem::path&,
                         const ReportDefault = ReportDefault::F) const;

    std::string
    persistConfiguration(const ReportDefault = ReportDefault::F) const;

    // end of outside api
    bool
    updateReadActivity();

    fungi::Mutex &getLock_();

    BackendInterfacePtr
    createBackendInterface(const Namespace&);

    double
    readActivity();

    void
    checkVolumeFailoverCaches();

    /*
     * Prevent concurrent configuration attempts.
     * Rather coarsely grained - concurrent readers don't pose any problem,
     * but are not permitted now.
     */
    fungi::Mutex mgmtMutex_;

    void
    setAsTemplate(const VolumeId);

    SCOCache*
    getSCOCache()
    {
        return &scoCache_;
    }

    ClusterCache&
    getClusterCache()
    {
        return ClusterCache_;
    }

    void
    getVolumeList(std::list<VolumeId> &) const;

    void
    getVolumesOverview(std::map<VolumeId, VolumeOverview>& out_map) const;

    const BackendConfig&
    getBackendConfig() const;

    fs::path
    getMetaDataPath() const
    {
         return fs::path(metadata_path.value());
    }

    fs::path
    getMetaDataPath(const Volume* v) const
    {
        return fs::path(metadata_path.value()) / v->getNamespace().str();
    }

    fs::path
    getMetaDataDBFilePath(const VolumeConfig& conf) const
    {
        return getMetaDataPath(conf) / metaDataDBName();
    }

    fs::path
    getMetaDataDBFilePath(const Volume* v) const
    {
        return getMetaDataPath(v) / metaDataDBName();
    }

    fs::path
    getMetaDataPath(const WriteOnlyVolume* v) const
    {
        return fs::path(metadata_path.value()) / v->getNamespace().str();
    }

    fs::path
    getMetaDataPath(const VolumeConfig& conf) const
    {
        return fs::path(metadata_path.value()) / conf.getNS().str();
    }

    fs::path
    getMetaDataPath(const backend::Namespace& ns) const
    {
        return fs::path(metadata_path.value()) / ns.str();
    }

    fs::path
    getTLogPath(const backend::Namespace& ns) const
    {
        return fs::path(tlog_path.value()) / ns.str();
    }

    fs::path
    getTLogPath() const
    {
        return fs::path(tlog_path.value());
    }

    fs::path
    getTLogPath(const Volume* v) const
    {
        return fs::path(tlog_path.value()) / v->getNamespace().str();
    }

    fs::path
    getTLogPath(const WriteOnlyVolume* v) const
    {
        return fs::path(tlog_path.value()) / v->getNamespace().str();
    }

    fs::path
    getTLogPath(const VolumeConfig& conf) const
    {
        return fs::path(tlog_path.value()) / conf.getNS().str();
    }

    fs::path
    getSnapshotsPath(const VolumeConfig& conf) const
    {
        return getMetaDataPath(conf)  / snapshotFilename();
    }

    fs::path
    getSnapshotsPath(const Volume* vol) const
    {
        return getMetaDataPath(vol)  / snapshotFilename();
    }

    unsigned
    getOpenSCOsPerVolume() const
    {
         return open_scos_per_volume.value();
    }

    template<typename T>
    std::unique_ptr<T>
    get_config_from_backend(const backend::Namespace& nspace)
    {
        backend::BackendInterfacePtr bi(createBackendInterface(nspace));
        return VolumeFactory::get_config_from_backend<T>(*bi);
    }

    void
    getClusterCacheDeviceInfo(ClusterCache::ManagerType::Info & info);

    void
    getClusterCacheStats(uint64_t& num_hits,
                         uint64_t& num_misses,
                         uint64_t& num_entries);

    void
    checkMetaDataFreeSpace();

    // XXX: return const bool& or get rid of it altogether and expose readOnlyMode_ or
    // require VolManager::get()?
    bool&
    readOnlyMode()
    {
        return readOnlyMode_;
    }

    template<typename T>
    void
    for_each_components(T& t)
    {
        for (auto& component : ::youtils::Notifier::components)
        {
            t(component);
        }
    }

    const backend::BackendConnectionManagerPtr
    getBackendConnectionManager()
    {
        return backend_conn_manager_;
    }

    uint64_t
    volumePotential(const SCOMultiplier,
                    const boost::optional<TLogMultiplier>&);

    uint64_t
    volumePotential(const backend::Namespace& ns);

    void
    checkSCOAndTLogMultipliers(const SCOMultiplier smult_old,
                               const SCOMultiplier smult_new,
                               const boost::optional<TLogMultiplier>& tmult_old,
                               const boost::optional<TLogMultiplier>& tmult_new);

    void
    setSCOMultiplier(const VolumeId&,
                     const SCOMultiplier);

    void
    setTLogMultiplier(const VolumeId&,
                      const boost::optional<TLogMultiplier>&);

    const Volume*
    findVolumeConst_(const VolumeId& volname,
                     const std::string& message = "Pity: ") const;

    const Volume*
    findVolumeConst_(const Namespace& ns) const;

    uint64_t
    get_sco_cache_max_non_disposable_bytes(const VolumeConfig&) const;

    // Slightly more than the actual configured limit - cf. "barts_correction" below.
    VolumeSize
    real_max_volume_size() const;

    // the number of bytes above 2TiB we allow on volume creation in order to tell our customers
    // that it only works up to 2TiB and check for it.
    const static uint64_t barts_correction = 67145728;

    events::PublisherPtr
    event_publisher() const
    {
        return event_publisher_;
    }

    uint64_t
    globalFDEstimate() const;

    uint64_t
    volumeFDEstimate(const unsigned num_volumes = 1) const;

    // Y42 push this down to youtils
    uint64_t
    backendFDEstimate() const;

    uint64_t
    getMaxFileDescriptors() const;

    std::shared_ptr<metadata_server::Manager>
    metadata_server_manager();

    ClusterCacheBehaviour
    get_cluster_cache_default_behaviour() const;

    ClusterCacheMode
    get_cluster_cache_default_mode() const;

private:
    DECLARE_LOGGER("VolManager");

    static VolManager* instance_;
    static fungi::Mutex instanceMutex_;

    events::PublisherPtr event_publisher_;
    VolPool backend_thread_pool_;

    VolumeMap volMap_;

    typedef std::map<Namespace, VolumeConfig> RestartMap;
    RestartMap restartMap_;

    time_t read_activity_time_;

    SCOCache scoCache_;

    boost::ptr_vector<youtils::PeriodicAction> periodicActions_;

    bool readOnlyMode_;

    backend::BackendConnectionManagerPtr backend_conn_manager_;

    ClusterCache ClusterCache_;

    std::shared_ptr<metadata_server::Manager> mds_manager_;

    mutable boost::optional<uint64_t> max_file_descriptors_;

    DECLARE_PARAMETER(metadata_path);
    DECLARE_PARAMETER(tlog_path);
    DECLARE_PARAMETER(open_scos_per_volume);
    DECLARE_PARAMETER(freespace_check_interval);
    DECLARE_PARAMETER(failovercache_check_interval_in_seconds);

private:
    DECLARE_PARAMETER(read_cache_default_behaviour);
    DECLARE_PARAMETER(read_cache_default_mode);

private:
    DECLARE_PARAMETER(clean_interval);
    DECLARE_PARAMETER(sap_persist_interval);
    DECLARE_PARAMETER(required_tlog_freespace);
    DECLARE_PARAMETER(required_meta_freespace);
    DECLARE_PARAMETER(max_volume_size);

public:
    DECLARE_PARAMETER(foc_throttle_usecs);
    DECLARE_PARAMETER(foc_queue_depth);
    DECLARE_PARAMETER(foc_write_trigger);

    DECLARE_PARAMETER(number_of_scos_in_tlog);
    DECLARE_PARAMETER(non_disposable_scos_factor);
    DECLARE_PARAMETER(debug_metadata_path);
    DECLARE_PARAMETER(arakoon_metadata_sequence_size);
    DECLARE_PARAMETER(allow_inconsistent_partial_reads);

private:
        /** @locking mgmtMutex_ must be locked */
    Volume*
    findVolume_noThrow_(const VolumeId&) const;

    void
    ensureVolumeNotPresent(const VolumeId&) const;

    void
    ensureVolumeNotPresent(const Namespace&) const;

    void
    ensureResourceLimits(const VolumeConfig&);

    void
    ensure_metadata_freespace_meticulously_(const VolumeConfig& cfg);

    void
    ensure_volume_size_(const VolumeSize) const;

    void
    ensureNamespaceNotRestarting(const Namespace&) const;

    Volume*
    local_restart(const Namespace& ns,
                  const OwnerTag,
                  const FallBackToBackendRestart,
                  const IgnoreFOCIfUnreachable,
                  const uint32_t num_pages_cached);

    uint64_t
    getQueueCount(const VolumeId& volName);

    uint64_t
    getQueueSize(const VolumeId& volName);

    bool
    checkEnoughFreeSpace_(const fs::path& p,
                          uint64_t minimal) const;

    bool
    check_enough_free_space_(const fs::path& p,
                             uint64_t min,
                             uint64_t warn_threshold) const;

    VolManager(const boost::property_tree::ptree& pt,
               events::PublisherPtr event_publisher);

    ~VolManager();

    void
    ensureNamespace(const Namespace& ns,
                    const CreateNamespace);

    void
    setNamespaceRestarting_(const Namespace& ns, const VolumeConfig& config);

    template<typename V>
    V*
    with_restart_map_and_unlocked_mgmt_(const std::function<V*(const Namespace& ns,
                                                               const VolumeConfig& cfg)>&,
                                        const Namespace& ns,
                                        const VolumeConfig& cfg);

    Volume*
    with_restart_map_and_unlocked_mgmt_vol_(const std::function<Volume*(const Namespace& ns,
                                                                  const VolumeConfig& cfg)>&,
                                            const Namespace& ns,
                                            const VolumeConfig& cfg);

    // VolumeDriverComponent Interface
    virtual const char*
    componentName() const override final
    {
        return initialized_params::volmanager_component_name;
    }

    virtual void
    update(const boost::property_tree::ptree&,
           UpdateReport& report) override;

    virtual void
    persist(boost::property_tree::ptree&,
            const ReportDefault reportDefault = ReportDefault::F) const override;

    virtual bool
    checkConfig(const boost::property_tree::ptree&,
                ConfigurationReport&) const override;

    Volume*
    findVolume_(const VolumeId& volname,
                const std::string& message = "Pity: ");

    Volume*
    findVolume_(const Namespace& ns);

    Volume*
    findVolumeFromNamespace(const Namespace& i_namespace) const;

    uint64_t
    getCurrentVolumesTLogRequirements();

    uint64_t
    getSCOCacheCapacityWithoutThrottling();

    uint64_t
    volumePotentialSCOCache(const SCOMultiplier,
                            const boost::optional<TLogMultiplier>&);

    uint64_t
    volumePotentialOpenFileDescriptors();
};

}

#endif // !VOLMANAGER_H_

// Local Variables: **
// mode: c++ **
// End: **
