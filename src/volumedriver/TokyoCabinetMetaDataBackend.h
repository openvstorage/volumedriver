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

#ifndef TOKYO_CABINET_META_DATA_BACKEND_H
#define TOKYO_CABINET_META_DATA_BACKEND_H

#include "CachedMetaDataStore.h"
#include "MetaDataBackendInterface.h"

#include <tcbdb.h>

#include <boost/filesystem.hpp>

namespace volumedrivertest
{
class TokyoCabinetMetaDataStoreTest;
}

namespace volumedriver
{

class TokyoCabinetMetaDataBackend
    : public MetaDataBackendInterface
{
    friend class ::volumedrivertest::TokyoCabinetMetaDataStoreTest;
public:
    explicit TokyoCabinetMetaDataBackend(const VolumeConfig& cfg,
                                         bool writer = true);

    explicit TokyoCabinetMetaDataBackend(const boost::filesystem::path& db_directory,
                                         bool writer = true);

    ~TokyoCabinetMetaDataBackend();

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

    virtual void
    set_delete_local_artefacts_on_destroy() noexcept
    {
        delete_local_artefacts = DeleteLocalArtefacts::T;
    }

    virtual void
    set_delete_global_artefacts_on_destroy() noexcept
    {
        delete_global_artefacts = DeleteGlobalArtefacts::T;
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

    youtils::UUID
    setFrozenParentCloneCork() override final
    {
        VERIFY(0 == "I'm a TokyoCabinetMetaDataBackend and don't know how to set a frozen parent clone cork");
    }

    std::unique_ptr<MetaDataBackendConfig>
    getConfig() const override final;

    MetaDataStoreFunctor&
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

    virtual MaybeScrubId
    scrub_id() override final;

    virtual void
    set_scrub_id(const ScrubId& id) override final;

    static uint64_t
    locally_used_bytes(const VolumeConfig&);

    static uint64_t
    locally_required_bytes(const VolumeConfig&);

private:
    DECLARE_LOGGER("TokyoCabinetMetaDataBackend");

    static const std::string db_name;

    const std::string nspace_;
    uint64_t used_clusters_;

    static const uint64_t cork_key_;
    static const uint64_t used_clusters_key_;
    static const uint64_t scrub_id_key_;

    bool
    getPage_(CachePage& p);

    TCBDB* database_;
    const fs::path filename_;

    static void
    fatalLog_(const char* message)
    {
        LOG_FATAL(message);
    }

    static uint64_t
    locally_required_bytes_(const VolumeConfig& cfg);

    static void
    check_page_address_(const CachePage&);

    DeleteLocalArtefacts delete_local_artefacts;
    DeleteGlobalArtefacts delete_global_artefacts;
};

}

#endif //TOKYO_CABINET_META_DATA_BACKEND_H

// Local Variables: **
// mode: c++ **
// End: **
