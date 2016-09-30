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

#ifndef VD_MDS_META_DATA_BACKEND_H_
#define VD_MDS_META_DATA_BACKEND_H_

#include "MetaDataBackendInterface.h"
#include "metadata-server/Interface.h"

#include <chrono>
#include <memory>

#include <boost/optional.hpp>

#include <youtils/Logging.h>

#include <backend/Namespace.h>

namespace volumedriver
{

class MDSNodeConfig;
class VolumeConfig;

// The correct name would be "MetaDataServerMetaDataBackend".
// Yours truly hopes that he can be forgiven for abbreviating the first part.
class MDSMetaDataBackend
    : public MetaDataBackendInterface
{
public:
    MDSMetaDataBackend(const MDSNodeConfig&,
                       const backend::Namespace&,
                       const boost::optional<OwnerTag>&,
                       const boost::optional<std::chrono::seconds>& timeout);

    MDSMetaDataBackend(metadata_server::DataBaseInterfacePtr,
                       const backend::Namespace&,
                       const OwnerTag);

    virtual ~MDSMetaDataBackend();

    MDSMetaDataBackend(const MDSMetaDataBackend&) = delete;

    MDSMetaDataBackend&
    operator=(const MDSMetaDataBackend&) = delete;

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

    void
    setCork(const youtils::UUID& cork_uuid) override final;

    boost::optional<youtils::UUID>
    lastCorkUUID() override final;

    inline uint64_t
    getUsedClusters() const override final
    {
        return used_clusters_;
    }

    bool
    freezeable() const override final
    {
        return false;
    }

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

    youtils::UUID
    setFrozenParentCloneCork() override final;

    MetaDataStoreFunctor&
    for_each(MetaDataStoreFunctor& f,
             const ClusterAddress max_pages) override final;

    std::unique_ptr<MetaDataBackendConfig>
    getConfig() const override final;

    virtual MaybeScrubId
    scrub_id() override final;

    virtual void
    set_scrub_id(const ScrubId& id) override final;

    static uint64_t
    locally_used_bytes(const VolumeConfig&)
    {
        return 0;
    }

    static uint64_t
    locally_required_bytes(const VolumeConfig&)
    {
        return 0;
    }

    void
    set_master()
    {
        VERIFY(owner_tag_);
        table_->set_role(metadata_server::Role::Master,
                         *owner_tag_);
    }

private:
    DECLARE_LOGGER("MDSMetaDataBackend");

    metadata_server::DataBaseInterfacePtr db_;
    metadata_server::TableInterfacePtr table_;
    const boost::optional<MDSNodeConfig> config_;
    uint64_t used_clusters_;
    const boost::optional<OwnerTag> owner_tag_;

    void
    init_();
};

}

#endif // !VD_MDS_META_DATA_BACKEND_H_
