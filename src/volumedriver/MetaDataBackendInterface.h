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
