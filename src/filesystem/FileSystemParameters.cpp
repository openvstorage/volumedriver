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

#include "FileSystemParameters.h"

namespace initialized_params
{

namespace vd = volumedriver;
namespace vfs = volumedriverfs;
namespace yt = youtils;

using namespace std::literals::string_literals;

// ObjectRouter:
const char volumerouter_component_name[] = "volume_router";

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_local_io_sleep_before_retry_usecs,
                                      volumerouter_component_name,
                                      "local_io_sleep_before_retry_usecs",
                                      "delay (microseconds) before rerrying a request that failed with a retryable error",
                                      ShowDocumentation::T,
                                      100000);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_local_io_retries,
                                      volumerouter_component_name,
                                      "vrouter_local_io_retries",
                                      "number of retry attempts for requests that failed with a retryable error",
                                      ShowDocumentation::T,
                                      600);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_check_local_volume_potential_period,
                                      volumerouter_component_name,
                                      "vrouter_check_local_volume_potential_period",
                                      "how often to recheck the local volume potential during migration",
                                      ShowDocumentation::T,
                                      1);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_volume_read_threshold,
                                      volumerouter_component_name,
                                      "vrouter_volume_read_threshold",
                                      "number of remote read requests before auto-migrating a volume - 0 turns it off",
                                      ShowDocumentation::T,
                                      0UL);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_volume_write_threshold,
                                      volumerouter_component_name,
                                      "vrouter_volume_write_threshold",
                                      "number of remote write requests before auto-migrating a volume - 0 turns it off",
                                      ShowDocumentation::T,
                                      0UL);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_file_read_threshold,
                                      volumerouter_component_name,
                                      "vrouter_file_read_threshold",
                                      "number of remote read requests before auto-migrating a file - 0 turns it off",
                                      ShowDocumentation::T,
                                      0UL);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_file_write_threshold,
                                      volumerouter_component_name,
                                      "vrouter_file_write_threshold",
                                      "number of remote write requests before auto-migrating a file - 0 turns it off",
                                      ShowDocumentation::T,
                                      0UL);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_redirect_timeout_ms,
                                      volumerouter_component_name,
                                      "vrouter_redirect_timeout_ms",
                                      "timeout for redirected requests in milliseconds - 0 turns it off",
                                      ShowDocumentation::T,
                                      0UL);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_backend_sync_timeout_ms,
                                      volumerouter_component_name,
                                      "vrouter_backend_sync_timeout_ms",
                                      "timeout for remote backend syncs (during migration) - 0 turns it off",
                                      ShowDocumentation::T,
                                      0UL);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_migrate_timeout_ms,
                                      volumerouter_component_name,
                                      "vrouter_migrate_timeout_ms",
                                      "timeout for migration requests in milliseconds (in addition to remote backend sync timeout!)",
                                      ShowDocumentation::T,
                                      500UL);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_redirect_retries,
                                      volumerouter_component_name,
                                      "vrouter_redirect_retries",
                                      "number of retries to after a redirect timed out",
                                      ShowDocumentation::T,
                                      10);

DEFINE_INITIALIZED_PARAM(vrouter_id,
                         volumerouter_component_name,
                         "vrouter_id",
                         "the vrouter_id of this node of the cluster. Must be one of the vrouter_id's s specified in the vrouter_cluster_nodes section",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_sco_multiplier,
                                      volumerouter_component_name,
                                      "sco_multiplier",
                                      "number of clusters in a sco",
                                      ShowDocumentation::T,
                                      1024);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_routing_retries,
                                      volumerouter_component_name,
                                      "vrouter_routing_retries",
                                      "number of times the routing shall be retried in case the volume is not found (exponential backoff inbetween retries!)",
                                      ShowDocumentation::T,
                                      30);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_lock_reaper_interval,
                                      volumerouter_component_name,
                                      "vrouter_lock_reaper_interval",
                                      "interval (in seconds) of the lock reaper",
                                      ShowDocumentation::F,
                                      60 * 60 * 8);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_min_workers,
                                      volumerouter_component_name,
                                      "vrouter_min_workers",
                                      "minimum number of worker threads to handle redirected requests",
                                      ShowDocumentation::T,
                                      4);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_max_workers,
                                      volumerouter_component_name,
                                      "vrouter_max_workers",
                                      "maximum number of worker threads to handle redirected requests",
                                      ShowDocumentation::T,
                                      16);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vrouter_registry_cache_capacity,
                                      volumerouter_component_name,
                                      "vrouter_registry_cache_capacity",
                                      "number of ObjectRegistrations to keep cached",
                                      ShowDocumentation::T,
                                      1024);

// ObjectRouterCluster
const char volumeroutercluster_component_name[] = "volume_router_cluster";

DEFINE_INITIALIZED_PARAM(vrouter_cluster_id,
                         volumeroutercluster_component_name,
                         "vrouter_cluster_id",
                         "cluster_id of the volumeroutercluster this node belongs to",
                         ShowDocumentation::T);

// FUSE:
const char fuse_component_name[] = "fuse";

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fuse_min_workers,
                                      fuse_component_name,
                                      "fuse_min_workers",
                                      "minimum number of FUSE worker threads",
                                      ShowDocumentation::T,
                                      8U);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fuse_max_workers,
                                      fuse_component_name,
                                      "fuse_max_workers",
                                      "maximum number of FUSE worker threads",
                                      ShowDocumentation::T,
                                      8U);

// FileSystem:
const char filesystem_component_name[] = "filesystem";

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_ignore_sync,
                                      filesystem_component_name,
                                      "fs_ignore_sync",
                                      "ignore sync requests - AT THE POTENTIAL EXPENSE OF DATA LOSS",
                                      ShowDocumentation::T,
                                      false);

DEFINE_INITIALIZED_PARAM(fs_virtual_disk_format,
                         filesystem_component_name,
                         "fs_virtual_disk_format",
                         "virtual disk format: vmdk or raw",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_raw_disk_suffix,
                                      filesystem_component_name,
                                      "fs_raw_disk_suffix",
                                      "Suffix to use when creating clones if fs_virtual_disk_format=raw",
                                      ShowDocumentation::T,
                                      ""s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_max_open_files,
                                      filesystem_component_name,
                                      "fs_max_open_files",
                                      "Maximum number of open files, is set using rlimit() on startup",
                                      ShowDocumentation::T,
                                      65536);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_internal_suffix,
                                      filesystem_component_name,
                                      "fs_internal_suffix",
                                      "suffix for filesystem internal files",
                                      ShowDocumentation::F,
                                      ".vfs_internal"s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_file_event_rules,
                                      filesystem_component_name,
                                      "fs_file_event_rules",
                                      "an array of filesystem event rules, each consisting of a \"path_regex\" and an array of \"fs_calls\"",
                                      ShowDocumentation::T,
                                      vfs::FileEventRules());

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_type,
                                      filesystem_component_name,
                                      "fs_metadata_backend_type",
                      "Type of metadata backend to use for volumes created via the filesystem interface",
                                      ShowDocumentation::T,
                                      vd::MetaDataBackendType::MDS);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_arakoon_cluster_id,
                                      filesystem_component_name,
                                      "fs_metadata_backend_arakoon_cluster_id",
                                      "Arakoon cluster identifier for the volume metadata",
                                      ShowDocumentation::T,
                                      ""s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_arakoon_cluster_nodes,
                                      filesystem_component_name,
                                      "fs_metadata_backend_arakoon_cluster_nodes",
                                      "an array of arakoon cluster node configurations for the volume metadata, each containing node_id, host and port",
                                      ShowDocumentation::T,
                                      arakoon::ArakoonNodeConfigs());

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_mds_nodes,
                                      filesystem_component_name,
                                      "fs_metadata_backend_mds_nodes",
                                      "an array of MDS node configurations for the volume metadata, each containing host and port",
                                      ShowDocumentation::T,
                                      vd::MDSNodeConfigs());

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_metadata_backend_mds_apply_relocations_to_slaves,
                                      filesystem_component_name,
                                      "fs_metadata_backend_mds_apply_relocations_to_slaves",
                                      "an bool indicating whether to apply relocations to slave MDS tables",
                                      ShowDocumentation::T,
                                      true);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_cache_dentries,
                                      filesystem_component_name,
                                      "fs_cache_dentries",
                                      "whether to cache directory entries locally",
                                      ShowDocumentation::T,
                                      false);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_nullio,
                                      filesystem_component_name,
                                      "fs_nullio",
                                      "discard any read/write/sync requests - for performance testing",
                                      ShowDocumentation::F,
                                      false);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_dtl_config_mode,
                                      filesystem_component_name,
                                      "fs_dtl_config_mode",
                                      "Configuration mode : Automatic | Manual",
                                      ShowDocumentation::T,
                                      vfs::FailOverCacheConfigMode::Automatic);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_dtl_host,
                                      filesystem_component_name,
                                      "fs_dtl_config",
                                      "DTL host",
                                      ShowDocumentation::T,
                                      ""s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_dtl_port,
                                      filesystem_component_name,
                                      "fs_dtl_config",
                                      "DTL port",
                                      ShowDocumentation::T,
                                      (uint16_t) 0);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(fs_dtl_mode,
                                      filesystem_component_name,
                                      "fs_dtl_mode",
                                      "DTL mode : Asynchronous | Synchronous",
                                      ShowDocumentation::T,
                                      vd::FailOverCacheMode::Asynchronous);

// EventPublisher
const char events_component_name[] = "event_publisher";

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(events_amqp_uris,
                                      events_component_name,
                                      "events_amqp_uris",
                                      "array of URIs, each consisting of an \"amqp_uri\" entry for a node of the AMQP cluster events shall be sent to",
                                      ShowDocumentation::T,
                                      vfs::AmqpUris());

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(events_amqp_exchange,
                                      events_component_name,
                                      "events_amqp_exchange",
                                      "AMQP exchange events will be sent to",
                                      ShowDocumentation::T,
                                      vfs::AmqpExchange(""));

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(events_amqp_routing_key,
                                      events_component_name,
                                      "events_amqp_routing_key",
                                      "AMQP routing key used for sending events",
                                      ShowDocumentation::T,
                                      vfs::AmqpRoutingKey(""));

//Failovercache
const char failovercache_component_name[] = "failovercache";

DEFINE_INITIALIZED_PARAM(failovercache_path,
                         failovercache_component_name,
                         "failovercache_path",
                         "path to the directory the failovercache writes its data in ",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(failovercache_transport,
                                      failovercache_component_name,
                                      "failovercache_transport",
                                      "transport to use for the failovercache server",
                                      ShowDocumentation::T,
                                      vd::FailOverCacheTransport::TCP);

// VolumeRegistry
const char volumeregistry_component_name[] = "volume_registry";

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(vregistry_arakoon_timeout_ms,
                                      volumeregistry_component_name,
                                      "vregistry_arakoon_timeout_ms",
                                      "Arakoon client timeout in milliseconds for the volume registry",
                                      ShowDocumentation::T,
                                      60000);

DEFINE_INITIALIZED_PARAM(vregistry_arakoon_cluster_id,
                         volumeregistry_component_name,
                         "vregistry_arakoon_cluster_id",
                         "Arakoon cluster identifier for the volume registry",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM(vregistry_arakoon_cluster_nodes,
                         volumeregistry_component_name,
                         "vregistry_arakoon_cluster_nodes",
                         "an array of arakoon cluster node configurations for the volume registry, each containing node_id, host and port",
                         ShowDocumentation::T);

}

// Local Variables: **
// mode: c++ **
// End: **
