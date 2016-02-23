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

#ifndef VOLUME_DRIVER_PARAMETERS_H
#define VOLUME_DRIVER_PARAMETERS_H

#include "ClusterCacheBehaviour.h"
#include "ClusterCacheMode.h"
#include "LockStoreType.h"
#include "MountPointConfig.h"
#include "Types.h"

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/InitializedParam.h>

namespace initialized_params
{

extern const char volmanager_component_name[];

DECLARE_INITIALIZED_PARAM(tlog_path, std::string);
DECLARE_INITIALIZED_PARAM(metadata_path, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(open_scos_per_volume, unsigned);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(dtl_throttle_usecs,
                                                  std::atomic<unsigned>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(dtl_queue_depth,
                                                  std::atomic<unsigned>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(dtl_write_trigger,
                                                  std::atomic<unsigned>);

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(freespace_check_interval,
                                                  std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM(clean_interval,
                                      std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(sap_persist_interval,
                                                  std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(dtl_check_interval_in_seconds,
                                                  std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(read_cache_default_behaviour,
                                                  volumedriver::ClusterCacheBehaviour);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(read_cache_default_mode,
                                                  volumedriver::ClusterCacheMode);
// TODO these should have a dimensioned value constructor.
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(required_meta_freespace,
                                                  std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(required_tlog_freespace,
                                                  std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(max_volume_size,
                                                  std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(allow_inconsistent_partial_reads,
                                                  std::atomic<bool>);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(number_of_scos_in_tlog,
                                       uint32_t);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(non_disposable_scos_factor,
                                       float);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(metadata_cache_capacity,
                                       uint32_t);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(debug_metadata_path, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(arakoon_metadata_sequence_size, uint32_t);

extern const char threadpool_component_name[];
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(num_threads, uint32_t);

extern const char scocache_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(datastore_throttle_usecs,
                                                  std::atomic<unsigned>);
DECLARE_INITIALIZED_PARAM(trigger_gap, youtils::DimensionedValue);
DECLARE_INITIALIZED_PARAM(backoff_gap, youtils::DimensionedValue);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(discount_factor, float);

DECLARE_INITIALIZED_PARAM(scocache_mount_points,
                          volumedriver::MountPointConfigs);

extern const char kak_component_name[];

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(serialize_read_cache, bool);
DECLARE_INITIALIZED_PARAM(read_cache_serialization_path, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(average_entries_per_bin, uint32_t);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(read_cache_cluster_size, uint32_t);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(clustercache_mount_points,
                                       volumedriver::MountPointConfigs);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(dls_type,
                                       volumedriver::LockStoreType);

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(dls_arakoon_cluster_nodes,
                                                  arakoon::ArakoonNodeConfigs);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(dls_arakoon_cluster_id, std::string);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(dls_arakoon_timeout_ms, uint32_t);

}

#endif // VOLUME_DRIVER_PARAMETERS_H

// Local Variables: **
// mode: c++ **
// End: **
