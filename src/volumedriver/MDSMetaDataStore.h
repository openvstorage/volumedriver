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

#ifndef VD_MDS_META_DATA_STORE_H_
#define VD_MDS_META_DATA_STORE_H_

#include "MetaDataStoreInterface.h"
#include "ScrubId.h"
#include "Types.h"
#include "VolumeBackPointer.h"

#include <boost/filesystem.hpp>

#include <youtils/IOException.h>
#include <youtils/Logging.h>
#include <youtils/RWLock.h>

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
    applyRelocs(RelocationReaderFactory&,
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

    virtual void
    set_cache_capacity(const size_t num_pages) override final;

    void
    set_config(const MDSMetaDataBackendConfig& cfg);

    MDSMetaDataBackendConfig
    get_config() const;

    MDSNodeConfig
    current_node() const;

    size_t
    incremental_rebuild_count() const
    {
        return incremental_rebuild_count_;
    }

    size_t
    full_rebuild_count() const
    {
        return full_rebuild_count_;
    }

private:
    DECLARE_LOGGER("MDSMetaDataStore");

    // locking:
    // * shared during I/O
    // * exclusive during error handling
    mutable fungi::RWLock rwlock_;
    std::shared_ptr<CachedMetaDataStore> mdstore_;
    const backend::BackendInterfacePtr bi_;

    // node_configs_[0] is the currently active one.
    // Rotated on failover.
    std::vector<MDSNodeConfig> node_configs_;
    ApplyRelocationsToSlaves apply_relocations_to_slaves_;
    std::chrono::seconds timeout_;

    const uint64_t num_pages_cached_;
    const boost::filesystem::path home_;

    size_t incremental_rebuild_count_;
    size_t full_rebuild_count_;

    using MetaDataStorePtr = std::shared_ptr<CachedMetaDataStore>;

    MetaDataStorePtr
    connect_(const MDSNodeConfig& ncfg) const;

    MetaDataStorePtr
    build_new_one_(const MDSNodeConfig& ncfg,
                   bool startup);

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
