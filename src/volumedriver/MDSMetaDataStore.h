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

#ifndef VD_MDS_META_DATA_STORE_H_
#define VD_MDS_META_DATA_STORE_H_

#include "MetaDataStoreInterface.h"
#include "ScrubId.h"
#include "Types.h"
#include "VolumeBackPointer.h"

#include <boost/filesystem.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>

#include <backend/BackendInterface.h>

namespace volumedriver
{

class CachedMetaDataStore;
class MDSMetaDataBackend;

class ClusterLocationAndHash;

class MDSMetaDataStore
    : public MetaDataStoreInterface
    , public VolumeBackPointer
{
public:
    MAKE_EXCEPTION(DeadAndGoneException,
                   MetaDataStoreException);

    MDSMetaDataStore(const MDSMetaDataBackendConfig& cfg,
                     backend::BackendInterfacePtr bi,
                     const boost::filesystem::path& home,
                     uint64_t num_pages_cached);

    ~MDSMetaDataStore() = default;

    MDSMetaDataStore(const MDSMetaDataStore&) = delete;

    MDSMetaDataStore&
    operator=(const MDSMetaDataStore&) = delete;

    virtual void
    initialize(VolumeInterface& vol) override final;

    virtual void
    readCluster(const ClusterAddress addr,
                ClusterLocationAndHash& loc) override;

    virtual void
    writeCluster(const ClusterAddress addr,
                 const ClusterLocationAndHash& loc) override;

    virtual void
    clear_all_keys() override;

    virtual void
    sync() override;

    // Set a flag to delete all local artefacts on destruction
    virtual void
    set_delete_local_artefacts_on_destroy() noexcept override;

    virtual void
    set_delete_global_artefacts_on_destroy() noexcept override;

    virtual void
    processCloneTLogs(const CloneTLogs& ctl,
                      const NSIDMap& nsidmap,
                      const fs::path& tlog_location,
                      bool sync,
                      const boost::optional<youtils::UUID>& uuid) override;

    virtual uint64_t
    applyRelocs(const std::vector<std::string>& relocs,
                const NSIDMap& nsid_map,
                const boost::filesystem::path& tlog_location,
                SCOCloneID,
                const ScrubId&) override final;

    virtual bool
    compare(MetaDataStoreInterface& other) override;

    virtual MetaDataStoreFunctor&
    for_each(MetaDataStoreFunctor& functor,
             const ClusterAddress max_loc) override;

    virtual void
    getStats(MetaDataStoreStats& stats) override;

    virtual boost::optional<youtils::UUID>
    lastCork() override;

    virtual void
    cork(const youtils::UUID& cork) override;

    virtual void
    unCork(const boost::optional<youtils::UUID>& cork) override;

    virtual void
    updateBackendConfig(const MetaDataBackendConfig& cfg) override;

    virtual std::unique_ptr<MetaDataBackendConfig>
    getBackendConfig() const override;

    virtual MaybeScrubId
    scrub_id() override;

    virtual void
    set_scrub_id(const ScrubId& id) override;

    void
    set_config(const MDSMetaDataBackendConfig& cfg);

    MDSMetaDataBackendConfig
    get_config() const;

    MDSNodeConfig
    current_node() const;

private:
    DECLARE_LOGGER("MDSMetaDataStore");

    // locking:
    // * shared during I/O
    // * exclusive during error handling
    mutable boost::shared_mutex rwlock_;
    std::shared_ptr<CachedMetaDataStore> mdstore_;
    const backend::BackendInterfacePtr bi_;

    // node_configs_[0] is the currently active one.
    // Rotated on failover.
    std::vector<MDSNodeConfig> node_configs_;
    ApplyRelocationsToSlaves apply_relocations_to_slaves_;
    std::chrono::seconds timeout_;

    const uint64_t num_pages_cached_;
    const boost::filesystem::path home_;

    using MetaDataStorePtr = std::shared_ptr<CachedMetaDataStore>;

    MetaDataStorePtr
    connect_(const MDSNodeConfig& ncfg) const;

    MetaDataStorePtr
    build_new_one_(const MDSNodeConfig& ncfg,
                   bool startup) const;

    void
    failover_(MetaDataStorePtr& md,
              const char* desc);

    // `startup' only serves to check the invariant that mdstore_ != nullptr
    // unless failover_ is invoked from the constructor.
    MetaDataStorePtr
    do_failover_(bool startup);

    template<typename R, typename... A>
    R
    handle_(const char* desc,
            R (MetaDataStoreInterface::*fn)(A... args),
            A... args);

    template<typename R, typename F>
    R
    do_handle_(const char* desc,
               F&& fun);

    MDSMetaDataBackendConfig
    get_config_() const;

    void
    check_config_(const MDSMetaDataBackendConfig&);

    void
    apply_relocs_on_slave_(const MDSNodeConfig& cfg,
                           const std::vector<std::string>& relocs,
                           SCOCloneID cid,
                           const ScrubId& scrub_id);
};

}

#endif // !VD_MDS_META_DATA_STORE_H_
