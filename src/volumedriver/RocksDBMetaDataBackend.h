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

#ifndef ROCKS_DB_META_DATA_BACKEND_H
#define ROCKS_DB_META_DATA_BACKEND_H

#include "CachedMetaDataStore.h"
#include "MetaDataBackendInterface.h"

namespace rocksdb
{
class DB;
}

namespace volumedrivertest
{
class TokyoCabinetMetaDataStoreTest;
}

namespace volumedriver
{

class RocksDBMetaDataBackend
    : public MetaDataBackendInterface
{
    friend class ::volumedrivertest::TokyoCabinetMetaDataStoreTest;
public:
    explicit RocksDBMetaDataBackend(const VolumeConfig& cfg,
                                    bool writer = true);

    ~RocksDBMetaDataBackend();

    bool
    getPage(CachePage& p) override final;

    void
    putPage(const CachePage& p,
            int32_t used_clusters_delta) override final;

    void
    discardPage(const CachePage& p,
                int32_t used_clusters_delta) override final;

    bool
    pageExistsInParent(const PageAddress) const override final
    {
        return false;
    }

    void
    sync() override final;

    void
    clear_all_keys() override final;

    bool
    freezeable() const override final
    {
        return false;
    }

    void
    setCork(const youtils::UUID& cork_uuid) override final;

    boost::optional<youtils::UUID>
    lastCorkUUID() override final;

    inline uint64_t
    getUsedClusters() const override final
    {
        return used_clusters_;
    }

    virtual youtils::UUID
    setFrozenParentCloneCork() override final
    {
        VERIFY(0 == "I'm a RocksDBMetaDataBackend and don't know how to set a frozen parent clone cork");
    }

    virtual MetaDataStoreFunctor&
    for_each(MetaDataStoreFunctor& f,
             const ClusterAddress max_pages) override final;

    bool
    hasFrozenParent() const override final
    {
        return false;
    }

    bool
    isEmancipated() const override final
    {
        return true;
    }

    std::unique_ptr<MetaDataBackendConfig>
    getConfig() const override final;

    virtual MaybeScrubId
    scrub_id() override final;

    virtual void
    set_scrub_id(const ScrubId& id) override final;

    void
    set_delete_local_artefacts_on_destroy() noexcept
    {
        delete_local_artefacts = DeleteLocalArtefacts::T;
    }

    void
    set_delete_global_artefacts_on_destroy() noexcept
    {
        delete_global_artefacts = DeleteGlobalArtefacts::T;
    }

    static uint64_t
    locally_used_bytes(const VolumeConfig&);

    static uint64_t
    locally_required_bytes(const VolumeConfig&);

private:
    DECLARE_LOGGER("RocksDBMetaDataBackend");

    static const std::string db_name;
    static const uint64_t cork_key_;
    static const uint64_t used_clusters_key_;
    static const uint64_t scrub_id_key_;

    std::unique_ptr<rocksdb::DB> db_;
    uint64_t used_clusters_;
    DeleteLocalArtefacts delete_local_artefacts;
    DeleteGlobalArtefacts delete_global_artefacts;

    const backend::Namespace nspace_;
    const fs::path filename_;

    static void
    fatalLog_(const char* message)
    {
        LOG_FATAL(message);
    }

    static uint64_t
    locally_required_bytes_(const VolumeConfig& cfg);

    static void
    check_page_address_(const PageAddress pa);
};

}

#endif //! ROCKS_DB_META_DATA_BACKEND_H

// Local Variables: **
// mode: c++ **
// End: **
