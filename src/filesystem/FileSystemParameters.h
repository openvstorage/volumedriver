// Copyright 2015 Open vStorage NV
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

#ifndef VFS_PARAMETERS_H_
#define VFS_PARAMETERS_H_

#include "AmqpTypes.h"
#include "ClusterNodeConfig.h"
#include "FileEventRule.h"
#include "FileSystemCall.h"
#include "NodeId.h"
#include "FailOverCacheConfigMode.h"

#include <atomic>

#include <youtils/ArakoonNodeConfig.h>
#include <youtils/InitializedParam.h>

#include <volumedriver/FailOverCacheTransport.h>
#include <volumedriver/MDSNodeConfig.h>
#include <volumedriver/MetaDataBackendConfig.h>
#include <volumedriver/FailOverCacheConfig.h>

namespace initialized_params
{

// ObjectRouter:
extern const char volumerouter_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_local_io_sleep_before_retry_usecs,
                                                  uint32_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_local_io_retries,
                                                  uint32_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_check_local_volume_potential_period,
                                                  std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_volume_read_threshold,
                                                  uint64_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_volume_write_threshold,
                                                  uint64_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_file_read_threshold,
                                                  uint64_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_file_write_threshold,
                                                  uint64_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_redirect_timeout_ms,
                                                  uint64_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_backend_sync_timeout_ms,
                                                  uint64_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_migrate_timeout_ms,
                                                  uint64_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_redirect_retries,
                                                  uint32_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_routing_retries,
                                                  uint32_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_lock_reaper_interval,
                                                  std::atomic<uint64_t>);
DECLARE_INITIALIZED_PARAM(vrouter_id,
                          std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_sco_multiplier,
                                       uint32_t);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_registry_cache_capacity,
                                       uint32_t);

// ObjectRouterCluster
// this section should be identical on all nodes of the cluster
extern const char volumeroutercluster_component_name[];

DECLARE_INITIALIZED_PARAM(vrouter_cluster_id, std::string);

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_min_workers, uint16_t);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_max_workers, uint16_t);

// FileSystem:
extern const char filesystem_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(fs_ignore_sync,
                                                  bool);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_max_open_files, uint32_t);
DECLARE_INITIALIZED_PARAM(fs_virtual_disk_format, std::string);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_raw_disk_suffix,
                                       std::string);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_internal_suffix,
                                       std::string);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_file_event_rules,
                                       volumedriverfs::FileEventRules);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_type,
                                       volumedriver::MetaDataBackendType);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_arakoon_cluster_nodes,
                                       arakoon::ArakoonNodeConfigs);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_arakoon_cluster_id,
                                       std::string);

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_mds_nodes,
                                                  volumedriver::MDSNodeConfigs);

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_mds_apply_relocations_to_slaves,
                                                  bool);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_cache_dentries,
                                       bool);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_nullio,
                                       bool);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_dtl_config_mode,
                                       volumedriverfs::FailOverCacheConfigMode);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_dtl_host,
                                       std::string);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_dtl_port,
                                       uint16_t);


// FUSE:
extern const char fuse_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(fuse_min_workers,
                                                  std::atomic<uint32_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(fuse_max_workers,
                                                  std::atomic<uint32_t>);

// EventPublisher:
extern const char events_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(events_amqp_uris,
                                                  volumedriverfs::AmqpUris);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(events_amqp_exchange,
                                                  volumedriverfs::AmqpExchange);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(events_amqp_routing_key,
                                                  volumedriverfs::AmqpRoutingKey);

//Failovercache
extern const char failovercache_component_name[];

DECLARE_INITIALIZED_PARAM(failovercache_path, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(failovercache_transport,
                                       volumedriver::FailOverCacheTransport);

// VolumeRegistry:
extern const char volumeregistry_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM(vregistry_arakoon_cluster_nodes,
                                     arakoon::ArakoonNodeConfigs);

DECLARE_INITIALIZED_PARAM(vregistry_arakoon_cluster_id, std::string);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vregistry_arakoon_timeout_ms, uint32_t);

}

#endif // !VFS_PARAMETERS_H_

// Local Variables: **
// mode: c++ **
// End: **
