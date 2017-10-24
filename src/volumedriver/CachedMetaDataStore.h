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

#ifndef CACHED_METADATA_STORE_H_
#define CACHED_METADATA_STORE_H_

#include "CachedMetaDataPage.h"
#include "MetaDataBackendInterface.h"
#include "MetaDataStoreInterface.h"
#include "PageSortingGenerator.h"
#include "ScrubId.h"
#include "Types.h"

#include <memory>

#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <youtils/Generator.h>
#include <youtils/Logging.h>

namespace volumedrivertest
{
class MetaDataStoreTest;
}

namespace volumedriver
{

class ClusterLocationAndHash;
class VolManagerTestSetup;

class CachedMetaDataStore
    : public MetaDataStoreInterface
{
    friend class VolManagerTestSetup;
    friend class volumedrivertest::MetaDataStoreTest;

public:
    CachedMetaDataStore(const MetaDataBackendInterfacePtr& backend,
                        const std::string& id,
                        uint64_t capacity = default_capacity_);

    virtual ~CachedMetaDataStore();

    CachedMetaDataStore(const CachedMetaDataStore&) = delete;

    CachedMetaDataStore&
    operator=(const CachedMetaDataStore&) = delete;

    youtils::UUID
    setFrozenParentCloneCork()
    {
        return backend_->setFrozenParentCloneCork();
    }

    virtual void
    initialize(VolumeInterface&) override final
    {}

    virtual void
    sync() override final
    {}

    virtual void
    readCluster(const ClusterAddress caddr,
                ClusterLocationAndHash& loc) override final;

    // must not be called concurrently by consumers.
    virtual void
    writeCluster(const ClusterAddress caddr,
                 const ClusterLocationAndHash& loc) override final;

    virtual void
    clear_all_keys() override final;

    // Y42 Delete Local Data may make no sense here
    // virtual void
    // destroy(DeleteLocalData delete_metadata)
    // {
    //     {
    //         LOCK_CORKS_WRITE;
    //         corks_.clear();
    //         cork_uuid_ = youtils::UUID::NullUUID();
    //     }
    //     write_dirty_pages_to_backend_and_clear_page_list(false, false);
    //     LOCK_BACKEND;
    //     backend_->destroy(delete_metadata);
    // }

    virtual bool
    compare(MetaDataStoreInterface& other) override final;

    virtual MetaDataStoreFunctor&
    for_each(MetaDataStoreFunctor& f,
             const ClusterAddress max_address) override final;

    virtual void
    getStats(MetaDataStoreStats& stats) override final;

    virtual boost::optional<youtils::UUID>
    lastCork() override final;

    virtual void
    cork(const youtils::UUID& cork) override final;

    virtual void
    unCork(const boost::optional<youtils::UUID>& cork) override final;

    virtual void
    processCloneTLogs(const CloneTLogs& ctl,
                      const NSIDMap& nsidmap,
                      const boost::filesystem::path& tlog_path,
                      bool sync,
                      const boost::optional<youtils::UUID>& uuid) override final;

    virtual ApplyRelocsResult
    applyRelocs(RelocationReaderFactory&,
                SCOCloneID,
                const ScrubId&) override final;

    virtual void
    set_delete_local_artefacts_on_destroy() noexcept override final;

    virtual void
    set_delete_global_artefacts_on_destroy() noexcept override final;

    virtual void
    updateBackendConfig(const MetaDataBackendConfig& cfg) override;

    virtual std::unique_ptr<MetaDataBackendConfig>
    getBackendConfig() const override final;

    virtual MaybeScrubId
    scrub_id() override final;

    virtual void
    set_scrub_id(const ScrubId& id) override final;

    virtual void
    set_cache_capacity(const size_t num_pages) override final;

    virtual std::vector<ClusterLocation>
    get_page(const ClusterAddress) override final;

    void
    discardCluster(const ClusterAddress caddr);

    bool
    hasFrozenParent() const
    {
        return backend_->hasFrozenParent();
    }

    bool
    isEmancipated() const
    {
        return backend_->isEmancipated();
    }

    bool
    freezeable() const;

    void
    getCorkedClusters(std::vector<std::pair<youtils::UUID, uint64_t> >& vec) const;

    static uint32_t
    default_capacity()
    {
        return default_capacity_;
    }

    uint32_t
    capacity() const
    {
        return pages_.size();
    }

    void
    copy_corked_entries_from(const CachedMetaDataStore& other);

    void
    drop_cache_including_dirty_pages()
    {
        LOG_WARN("dropping all cache pages, even the dirty ones!");
        write_dirty_pages_to_backend_and_clear_page_list(false,
                                                         true);
    }

    // not const as VolManagerRestartTest.testAllTlogEntriesAreReplayed messes with it
    static uint64_t replayClustersCached;
    static const uint32_t replayPagesQueued;

private:
    DECLARE_LOGGER("CachedMetaDataStore");

    MetaDataBackendInterfacePtr backend_;

    std::vector<CachePage> pages_;
    std::vector<ClusterLocationAndHash> page_data_;

    // For now "num_pages_" tracks the size of the map as its ->size() is not O(1).
    // However, we only make use of auto-unlinking (which necessitates our use of
    // constant_time_size<false>), or more precisely an explicit
    // page->unlink_from_set(), just before we have to go to the backend in the hot
    // path. So it might be acceptable to use constant_time_size<true> instead and
    // get rid of "num_pages_" which has the potential of being out of sync if not
    // treated carefully.
    typedef bi::set<CachePage,
                    bi::constant_time_size<false> > map_type;

    typedef bi::list<CachePage,
                     bi::constant_time_size<false> > list_type;

    map_type page_map_;
    list_type page_list_;
    uint64_t num_pages_;
    uint64_t cache_hits_;
    uint64_t cache_misses_;
    uint64_t written_clusters_;
    uint64_t discarded_clusters_;

    const std::string id_;

    static const uint64_t default_capacity_ = 1024;

#ifndef NDEBUG
    mutable boost::mutex uncork_dbg_mutex_;
#endif

    // Ze locks - to be taken in this very order.
    mutable boost::shared_mutex corks_lock_;
    mutable boost::shared_mutex cache_lock_;
    mutable boost::mutex backend_lock_;

    boost::optional<youtils::UUID> cork_uuid_;
    MaybeScrubId scrub_id_;

    typedef std::map<ClusterAddress, ClusterLocationAndHash> ca_loc_map_type;
    typedef std::shared_ptr<ca_loc_map_type> ca_loc_map_ptr_type;
    typedef std::pair<youtils::UUID, ca_loc_map_ptr_type> cork_t;
    // orders latest cork at the end
    typedef std::list<cork_t> corks_t;

    corks_t corks_;

    uint64_t
    processPages(std::unique_ptr<youtils::Generator<PageDataPtr>> r,
                 SCOCloneID cloneid);

    void
    init_pages_(size_t capacity);

    // bit of a misnomer - if sync is false nothing is written out!
    void
    write_dirty_pages_to_backend_and_clear_page_list(bool sync,
                                                     bool ignore_errors = false);

    void
    do_write_dirty_pages_to_backend_and_clear_page_list(bool sync,
                                                        bool ignore_errors);

    void
    write_dirty_pages_to_backend_keeping_page_list();

    bool
    get_cluster_location_unlocked_(const ClusterAddress ca,
                                   ClusterLocationAndHash& loc,
                                   bool for_write);

    bool
    get_cluster_location_(const ClusterAddress,
                          ClusterLocationAndHash&,
                          bool for_write);

    std::pair<CachePage*, bool>
    get_page_(const ClusterAddress);

    typedef void (MetaDataBackendInterface::*backend_mem_fun)(const CachePage&,
                                                              int32_t);

    void
    dispose_page(backend_mem_fun dispose, CachePage& p);

    // TODO: rather expensive - can we be more clever?
    bool
    possibly_discard_page_(CachePage& p);

    bool
    maybeWritePage_locked_context(CachePage& p,
                                  bool ignore_errors);

    uint64_t
    processTLogReaderInterface(std::shared_ptr<TLogReaderInterface> r,
                               SCOCloneID cloneid);

#ifndef NDEBUG
    virtual void
    _print_corks_();
#endif

};


}

#endif // !CACHED_METADATA_STORE_H_

// Local Variables: **
// mode: c++ **
// End: **
