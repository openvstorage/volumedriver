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

#include "LockStoreFactory.h"
#include "VolumeDriverParameters.h"

namespace initialized_params
{

namespace ara = arakoon;
namespace vd = volumedriver;
namespace yt = youtils;

using namespace std::literals::string_literals;

const char threadpool_component_name[] = "threadpool_component";

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(num_threads,
                                      threadpool_component_name,
                                      "backend_threads",
                                      "Number of threads writting SCOs to the backend",
                                      ShowDocumentation::T,
                                      4);

extern const char volmanager_component_name[] = "volume_manager";

DEFINE_INITIALIZED_PARAM(metadata_path,
                         volmanager_component_name,
                         "metadata_path",
                         "Directory, where to create subdirectories in for volume metadata storage",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM(tlog_path,
                         volmanager_component_name,
                         "tlog_path",
                         "Directory, where to create subdirectories for volume tlogs",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(open_scos_per_volume,
                                      volmanager_component_name,
                                      "openSCOs",
                                      "Number of open SCOs per volume",
                                      ShowDocumentation::T,
                                      32);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(dtl_throttle_usecs,
                                      volmanager_component_name ,
                                      "dtl_retry_usecs",
                                      "Timeout for retrying writes to the DTL",
                                      ShowDocumentation::T,
                                      1000);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(dtl_queue_depth,
                                      volmanager_component_name,
                                      "dtl_queue_depth",
                                      "Size of the queue of entries to be sent to the DTL",
                                      ShowDocumentation::T,
                                      1024);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(dtl_write_trigger,
                                      volmanager_component_name,
                                      "dtl_write_trigger",
                                      "Trigger to start writing entries in the foc queue to the backend",
                                      ShowDocumentation::T,
                                      8);

DEFINE_INITIALIZED_PARAM(clean_interval,
                         volmanager_component_name,
                         "scocache_cleanup_interval",
                         "Interval between runs of scocache cleanups, in seconds.\n"
                         "Should be small when running on ramdisk, larger when running on sata.\n"
                         "scocache_cleanup_trigger / clean_interval should be larger than the aggregated write speed to the scocache.",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(sap_persist_interval,
                                      volmanager_component_name,
                                      "sap_persist_interval",
                                      "Interval between writing SAP data, in seconds",
                                      ShowDocumentation::T,
                                      300);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(dtl_check_interval_in_seconds,
                                      volmanager_component_name,
                                      "dtl_check_interval_in_seconds",
                                      "Interval between checks of the DTL state of volumes",
                                      ShowDocumentation::T,
                                      5 * 60);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(read_cache_default_behaviour,
                                      volmanager_component_name,
                                      "read_cache_default_behaviour",
                                      "Default read cache behaviour, should be CacheOnWrite, CacheOnRead or NoCache",
                                      ShowDocumentation::T,
                                      volumedriver::ClusterCacheBehaviour::CacheOnRead);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(read_cache_default_mode,
                                      volmanager_component_name,
                                      "read_cache_default_mode",
                                      "Default read cache mode, should be ContentBased or LocationBased",
                                      ShowDocumentation::T,
                                      volumedriver::ClusterCacheMode::ContentBased);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(required_tlog_freespace,
                                      volmanager_component_name,
                                      "required_tlog_freespace",
                                      "Required free space in the tlog directory",
                                      ShowDocumentation::T,
                                      200ULL << 20);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(required_meta_freespace,
                                      volmanager_component_name,
                                      "require_meta_freespace",
                                      "Required free space in the metadata directory",
                                      ShowDocumentation::T,
                                      200ULL << 20);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(max_volume_size,
                                      volmanager_component_name,
                                      "max_volume_size",
                                      "Maximum size for volumes - checked on creation and resize",
                                      ShowDocumentation::F,
                                      2ULL << 40);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(allow_inconsistent_partial_reads,
                                      volmanager_component_name,
                                      "allow_inconsistent_partial_reads",
                                      "Whether partial reads from the backend have to be consistent",
                                      ShowDocumentation::F,
                                      true);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(freespace_check_interval,
                                      volmanager_component_name,
                                      "freespace_check_interval",
                                      "Interval between checks of required freespace parameters, in seconds",
                                      ShowDocumentation::T,
                                      10);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(number_of_scos_in_tlog,
                                      volmanager_component_name,
                                      "number_of_scos_in_tlog",
                                      "The number of SCOs that trigger a tlog rollover",
                                      ShowDocumentation::T,
                                      20);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(non_disposable_scos_factor,
                                      volmanager_component_name,
                                      "non_disposable_scos_factor",
                                      "Factor to multiply number_of_scos_in_tlog with to determine the amount of non-disposable data permitted per volume",
                                      ShowDocumentation::T,
                                      1.5);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(default_cluster_size,
                                      volmanager_component_name,
                                      "default_cluster_size",
                                      "size of a cluster in bytes",
                                      ShowDocumentation::T,
                                      4096U);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(metadata_cache_capacity,
                                      volmanager_component_name,
                                      "metadata_cache_capacity",
                                      "number of metadata pages to keep cached",
                                      ShowDocumentation::T,
                                      8192);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(debug_metadata_path,
                                      volmanager_component_name,
                                      "no_python_name",
                                      "place to store evidence when a volume is halted.",
                                      ShowDocumentation::T,
                                      "/opt/qbase3/var/lib/volumedriver/evidence"s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(arakoon_metadata_sequence_size,
                                      volmanager_component_name,
                                      "arakoon_metadata_sequence_size",
                                      "Size of Arakoon sequences used to send metadata pages to Arakoon",
                                      ShowDocumentation::T,
                                      10);

const char scocache_component_name[] = "scocache";

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(datastore_throttle_usecs,
                                      scocache_component_name,
                                      "throttle_usecs",
                                      "No idea",
                                      ShowDocumentation::F,
                                      4000);

DEFINE_INITIALIZED_PARAM(trigger_gap,
                         scocache_component_name,
                         "scocache_cleanup_trigger",
                         "scocache-mountpoint freespace threshold below which scocache-cleaner is triggered",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM(backoff_gap,
                         scocache_component_name,
                         "scocache_cleanup_backoff",
                         "scocache-mountpoint freespace objective for scocache-cleaner",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(discount_factor,
                                      scocache_component_name,
                                      "discount_factor",
                                      "No idea",
                                      ShowDocumentation::F,
                                      1e-6);

DEFINE_INITIALIZED_PARAM(scocache_mount_points,
                         scocache_component_name,
                         "scocache_mountpoints",
                         "An array of directories and sizes to be used as scocache mount points",
                         ShowDocumentation::T);

const char kak_component_name[] = "content_addressed_cache";

DEFINE_INITIALIZED_PARAM(read_cache_serialization_path,
                         kak_component_name,
                         "readcache_serialization_path",
                         "Directory to store the serialization of the Read Cache",
                         ShowDocumentation::T);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(average_entries_per_bin,
                                      kak_component_name,
                                      "average_entries_per_bin",
                                      "Average size of the buckets in the Read Cache Hash, just keep the default",
                                      ShowDocumentation::F,
                                      2);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(serialize_read_cache,
                                      kak_component_name,
                                      "serialize_readcache",
                                      "Whether to serialize the readcache on exit or not",
                                      ShowDocumentation::T,
                                      true);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(clustercache_mount_points,
                                      kak_component_name,
                                      "clustercache_devices",
                                      "An array of directories and sizes to be used as Read Cache mount points",
                                      ShowDocumentation::T,
                                      vd::MountPointConfigs());

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(dls_type,
                                      vd::LockStoreFactory::name(),
                                      "dls_type",
                                      "Type of distributed lock store to use (default / currently only supported value: \"Backend\")",
                                      ShowDocumentation::T,
                                      vd::LockStoreType::Backend);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(dls_arakoon_timeout_ms,
                                      vd::LockStoreFactory::name(),
                                      "dls_arakoon_timeout_ms",
                                      "Arakoon client timeout in milliseconds for the distributed lock store",
                                      ShowDocumentation::T,
                                      60000);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(dls_arakoon_cluster_id,
                                      vd::LockStoreFactory::name(),
                                      "dls_arakoon_cluster_id",
                                      "Arakoon cluster identifier for the distributed lock store",
                                      ShowDocumentation::T,
                                      ""s);

DEFINE_INITIALIZED_PARAM_WITH_DEFAULT(dls_arakoon_cluster_nodes,
                                      vd::LockStoreFactory::name(),
                                      "dls_arakoon_cluster_nodes",
                                      "an array of arakoon cluster node configurations for the distributed lock store, each containing node_id, host and port",
                                      ShowDocumentation::T,
                                      ara::ArakoonNodeConfigs());

}

// Local Variables: **
// mode: c++ **
// End: **
