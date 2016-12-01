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

#ifndef METADATA_STORE_INTERFACE_H_
#define METADATA_STORE_INTERFACE_H_

#include "ScrubId.h"
#include "Types.h"
#include "MetaDataStoreStats.h"

#include <youtils/IOException.h>

namespace volumedriver
{

class ClusterLocationAndHash;
class MetaDataBackendConfig;
class NSIDMap;
class RelocationReaderFactory;
class TLogReaderInterface;
class VolumeInterface;

VD_BOOLEAN_ENUM(DeleteLocalArtefacts);
VD_BOOLEAN_ENUM(DeleteGlobalArtefacts);

MAKE_EXCEPTION(MetaDataStoreException, fungi::IOException);
MAKE_EXCEPTION(UpdateMetaDataBackendConfigException, MetaDataStoreException);

// badly named
class MetaDataStoreFunctor
{
public:
    virtual
    ~MetaDataStoreFunctor()
    {};

    virtual void
    operator()(ClusterAddress, const ClusterLocationAndHash&) = 0;
};

class MetaDataStoreInterface
{
public:
    MetaDataStoreInterface() noexcept
    {}

    virtual
    ~MetaDataStoreInterface() = default;

    virtual void
    initialize(VolumeInterface& v) = 0;

    virtual void
    readCluster(const ClusterAddress addr,
                ClusterLocationAndHash& loc) = 0;

    virtual void
    writeCluster(const ClusterAddress addr,
                 const ClusterLocationAndHash& loc) = 0;

    virtual void
    clear_all_keys() = 0;

    virtual void
    sync() = 0;

    // Set a flag to delete all local artefacts on destruction
    virtual void
    set_delete_local_artefacts_on_destroy() noexcept = 0;

    virtual void
    set_delete_global_artefacts_on_destroy() noexcept = 0;

    virtual void
    processCloneTLogs(const CloneTLogs& ctl,
                      const NSIDMap& nsidmap,
                      const boost::filesystem::path& tlog_location,
                      bool sync,
                      const boost::optional<youtils::UUID>& uuid) = 0;

    virtual uint64_t
    applyRelocs(RelocationReaderFactory&,
                SCOCloneID,
                const ScrubId&) = 0;

    virtual bool
    compare(MetaDataStoreInterface& other) = 0;

    virtual MetaDataStoreFunctor&
    for_each(MetaDataStoreFunctor& functor,
             const ClusterAddress max_loc) = 0;

    virtual void
    getStats(MetaDataStoreStats& stats) = 0;

    virtual boost::optional<youtils::UUID>
    lastCork() = 0;

    virtual void
    cork(const youtils::UUID& cork) = 0;

    virtual void
    unCork(const boost::optional<youtils::UUID>& cork = boost::none) = 0;

    virtual void
    updateBackendConfig(const MetaDataBackendConfig& cfg) = 0;

    virtual std::unique_ptr<MetaDataBackendConfig>
    getBackendConfig() const = 0;

    virtual MaybeScrubId
    scrub_id() = 0;

    virtual void
    set_scrub_id(const ScrubId&) = 0;

    virtual void
    set_cache_capacity(const size_t npages) = 0;

    virtual std::vector<ClusterLocationAndHash>
    get_page(const ClusterAddress) = 0;
};

}

#endif // !METADATASTORE_INTERFACE_H_

// Local Variables: **
// mode: c++ **
// End: **
