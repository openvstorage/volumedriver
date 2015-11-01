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
class TLogReaderInterface;
class VolumeInterface;

BOOLEAN_ENUM(DeleteLocalArtefacts);
BOOLEAN_ENUM(DeleteGlobalArtefacts);

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
    applyRelocs(const std::vector<std::string>& reloc_logs,
                const NSIDMap& nsid_map,
                const boost::filesystem::path& tlog_location,
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
};

}

#endif // !METADATASTORE_INTERFACE_H_

// Local Variables: **
// mode: c++ **
// End: **
