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

#ifndef VOLUME_FACTORY_H
#define VOLUME_FACTORY_H

#include "Entry.h"
#include "MetaDataStoreInterface.h"
#include "RestartContext.h"
#include "Types.h"
#include "VolumeConfig.h"

#include <list>

#include <boost/filesystem.hpp>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/ArakoonInterface.h>
#include <youtils/Logging.h>

namespace volumedriver
{

namespace fs = boost::filesystem;

class Volume;
class VolumeDriverCache;
class RemoteVolumeDriverCache;
class NSIDMap;
class DataStoreNG;
class MetaDataStoreInterface;
class WriteOnlyVolume;
class LocalRestartData;
class SnapshotPersistor;

struct VolumeFactory
{
    static std::unique_ptr<Volume>
    createNewVolume(const VolumeConfig&);

    static std::unique_ptr<WriteOnlyVolume>
    createWriteOnlyVolume(const VolumeConfig&);

    static std::unique_ptr<Volume>
    createClone(const VolumeConfig&,
                const PrefetchVolumeData,
                const youtils::UUID& parent_snap_uuid);

    static std::unique_ptr<Volume>
    local_restart(const VolumeConfig&,
                  const OwnerTag,
                  const FallBackToBackendRestart,
                  const IgnoreFOCIfUnreachable);

    static std::unique_ptr<Volume>
    backend_restart(const VolumeConfig&,
                    const OwnerTag,
                    const PrefetchVolumeData,
                    const IgnoreFOCIfUnreachable);

    static std::unique_ptr<WriteOnlyVolume>
    backend_restart_write_only_volume(const VolumeConfig&,
                                      const OwnerTag);

    static uint64_t
    metadata_locally_used_bytes(const VolumeConfig&);

    static uint64_t
    metadata_locally_required_bytes(const VolumeConfig&);

    template<typename T>
    static std::unique_ptr<T>
    get_config_from_backend(backend::BackendInterface& bi)
    {
        auto cfg(std::make_unique<T>());
        bi.fillObject(*cfg,
                      T::config_backend_name,
                      InsistOnLatestVersion::T);
        return cfg;
    }

private:
    friend class VolManagerTestSetup;

    static std::unique_ptr<Volume>
    local_restart_volume(const VolumeConfig&,
                         const OwnerTag,
                         const IgnoreFOCIfUnreachable);

    static boost::shared_ptr<backend::Condition>
    claim_namespace(const backend::Namespace&,
                    const OwnerTag);
};

} // namespace volumedriver

#endif // VOLUME_FACTORY_H

// Local Variables: **
// mode: c++ **
// End: **
