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

#ifndef VD_MDS_META_DATA_BACKEND_H_
#define VD_MDS_META_DATA_BACKEND_H_

#include "MetaDataBackendInterface.h"
#include "metadata-server/Interface.h"

#include <memory>

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
    MDSMetaDataBackend(const MDSNodeConfig& config,
                       const backend::Namespace& nspace);

    MDSMetaDataBackend(metadata_server::DataBaseInterfacePtr db,
                       const backend::Namespace& nspace);

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
        table_->set_role(metadata_server::Role::Master);
    }

private:
    DECLARE_LOGGER("MDSMetaDataBackend");

    metadata_server::DataBaseInterfacePtr db_;
    metadata_server::TableInterfacePtr table_;
    const boost::optional<MDSNodeConfig> config_;
    uint64_t used_clusters_;

    void
    init_();
};

}

#endif // !VD_MDS_META_DATA_BACKEND_H_
