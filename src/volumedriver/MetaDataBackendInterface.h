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

#ifndef META_DATA_BACKEND_INTERFACE
#define META_DATA_BACKEND_INTERFACE

#include "MetaDataStoreInterface.h"
#include "ScrubId.h"
#include "Types.h"

#include <youtils/IOException.h>

namespace volumedriver
{

MAKE_EXCEPTION(MetaDataStoreBackendException, fungi::IOException);
class CachePage;

class MetaDataBackendInterface
{
public:
    MetaDataBackendInterface() noexcept
        : delete_local_artefacts(DeleteLocalArtefacts::F)
        , delete_global_artefacts(DeleteGlobalArtefacts::F)
    {}

    virtual ~MetaDataBackendInterface() = default;

    virtual bool
    getPage(CachePage& p) = 0;

    virtual bool
    isEmancipated() const = 0;

    virtual void
    putPage(const CachePage& p,
            int32_t used_clusters_delta) = 0;

    virtual void
    discardPage(const CachePage& p,
                int32_t used_clusters_delta) = 0;

    virtual bool
    pageExistsInParent(const PageAddress) const = 0;

    virtual void
    sync() = 0;

    virtual void
    clear_all_keys() = 0;

    virtual MaybeScrubId
    scrub_id() = 0;

    virtual void
    set_scrub_id(const ScrubId& id) = 0;

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

    virtual void
    setCork(const youtils::UUID& cork_uuid) = 0;

    virtual uint64_t
    getUsedClusters() const = 0;

    virtual boost::optional<youtils::UUID>
    lastCorkUUID() = 0;

    virtual bool
    freezeable() const = 0;

    virtual bool
    hasFrozenParent() const = 0;

    virtual MetaDataStoreFunctor&
    for_each(MetaDataStoreFunctor& f,
             const ClusterAddress) = 0;

    virtual youtils::UUID
    setFrozenParentCloneCork() = 0;

    virtual std::unique_ptr<MetaDataBackendConfig>
    getConfig() const = 0;

    DeleteLocalArtefacts delete_local_artefacts;
    DeleteGlobalArtefacts delete_global_artefacts;
};

class MetaDataStoreFunctor;

typedef std::shared_ptr<MetaDataBackendInterface> MetaDataBackendInterfacePtr;

}

#endif

// Local Variables: **
// mode: c++ **
// End: **
