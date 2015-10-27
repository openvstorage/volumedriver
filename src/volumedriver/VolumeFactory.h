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
    createNewVolume(const VolumeConfig& config,
                    const uint32_t metadata_cache_pages);

    static std::unique_ptr<WriteOnlyVolume>
    createWriteOnlyVolume(const VolumeConfig& config);

    static std::unique_ptr<Volume>
    createClone(const VolumeConfig&,
                const PrefetchVolumeData,
                const uint32_t metadata_cache_pages,
                const youtils::UUID& parent_snap_uuid);

    static std::unique_ptr<Volume>
    local_restart(const VolumeConfig& config,
                  const OwnerTag owner_tag,
                  const FallBackToBackendRestart,
                  const IgnoreFOCIfUnreachable,
                  const uint32_t num_pages_cached);

    static std::unique_ptr<Volume>
    backend_restart(const VolumeConfig& volume_config,
                    const OwnerTag owner_tag,
                    const PrefetchVolumeData,
                    const IgnoreFOCIfUnreachable,
                    const uint32_t num_pages_cached);

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
};

} // namespace volumedriver

#endif // VOLUME_FACTORY_H

// Local Variables: **
// mode: c++ **
// End: **
