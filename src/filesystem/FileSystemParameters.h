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

#include <volumedriver/DtlTransport.h>
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

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_mds_timeout_secs,
                                                  uint32_t);

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

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_dtl_mode,
                                       volumedriver::FailOverCacheMode);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_enable_shm_interface,
                                       bool);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(fs_enable_network_interface,
                                       bool);

// FUSE:
extern const char fuse_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(fuse_min_workers,
                                                  std::atomic<uint32_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(fuse_max_workers,
                                                  std::atomic<uint32_t>);

// SHM:
extern const char shm_interface_component_name[];

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(shm_region_size,
                                       size_t);

// NETWORK:
extern const char network_interface_component_name[];

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(network_uri,
                                       std::string);

DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(network_snd_rcv_queue_depth,
                                       size_t);

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

DECLARE_INITIALIZED_PARAM(dtl_path, std::string);
DECLARE_INITIALIZED_PARAM_WITH_DEFAULT(dtl_transport,
                                       volumedriver::DtlTransport);

// VolumeRegistry:
extern const char volumeregistry_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM(vregistry_arakoon_cluster_nodes,
                                     arakoon::ArakoonNodeConfigs);

DECLARE_INITIALIZED_PARAM(vregistry_arakoon_cluster_id, std::string);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(vregistry_arakoon_timeout_ms, uint32_t);

// ScrubManager
extern const char scrub_manager_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(scrub_manager_interval,
                                                  std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(scrub_manager_sync_wait_secs,
                                                  std::atomic<uint64_t>);

// StatsCollector
extern const char stats_collector_component_name[];

DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(stats_collector_interval_secs,
                                                  std::atomic<uint64_t>);
DECLARE_RESETTABLE_INITIALIZED_PARAM_WITH_DEFAULT(stats_collector_destination,
                                                  std::string);
}

#endif // !VFS_PARAMETERS_H_

// Local Variables: **
// mode: c++ **
// End: **
