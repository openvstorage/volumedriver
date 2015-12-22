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

#ifndef VFS_XMLRPC_KEYS_H_
#define VFS_XMLRPC_KEYS_H_

#include <string>

namespace volumedriverfs
{
using namespace std::string_literals;
// Push this down to the volumedriverlib (or another intermediate lib) so it can
// be reused by the daemon as well.
struct XMLRPCKeys
{
    static const std::string backend_size;
    static const std::string backend_type;
    static const std::string choking;
    static const std::string cleanup_on_success;
    static const std::string cleanup_on_error;
    static const std::string cluster_cache_behaviour;
    static const std::string cluster_cache_handle;
    static const std::string cluster_cache_hits;
    static const std::string cluster_cache_limit;
    static const std::string cluster_cache_misses;
    static const std::string cluster_cache_mode;
    static const std::string cluster_exponent;
    static const std::string cluster_multiplier;
    static const std::string code;
    static const std::string component_name;
    static const std::string configuration;
    static const std::string configuration_path;
    static const std::string current_sco_count;
    static const std::string data_store_read_used;
    static const std::string data_store_write_used;
    static const std::string delay;
    static const std::string delete_global_metadata;
    static const std::string delete_local_data;
    static const std::string device_info;
    static const std::string disposable;
    static const std::string end_snapshot;
    static const std::string entries;
    static const std::string errors;
    static const std::string error_string;
    static const std::string failover_ip;
    static const std::string failover_mode;
    static const std::string failover_port;
    static const std::string foc_config;
    static const std::string foc_config_mode;
    static const std::string file_name;
    static const std::string footprint;
    static const std::string force;
    static const std::string free;
    static const std::string halted;
    static const std::string hits;
    static const std::string ignore_foc_if_unreachable;
    static const std::string lba_count;
    static const std::string lba_size;
    static const std::string local_path;
    static const std::string log_filter_level;
    static const std::string log_filter_name;
    static const std::string map_stats;
    static const std::string max_size;
    static const std::string max_non_disposable_factor;
    static const std::string max_non_disposable_size;
    static const std::string maximum_time_to_ignore_syncs_in_seconds;
    static const std::string metadata;
    static const std::string metadata_backend_config;
    static const std::string metadata_cache_capacity;
    static const std::string metadata_store_hits;
    static const std::string metadata_store_misses;
    static const std::string migrate_cache_to_parent;
    static const std::string min_size;
    static const std::string misc_info;
    static const std::string misses;
    static const std::string nspace;
    static const std::string new_value;
    static const std::string non_disposable;
    static const std::string no_updates;
    static const std::string number_of_syncs_to_ignore;
    static const std::string offlined;
    static const std::string old_value;
    static const std::string param_name;
    static const std::string parent_nspace;
    static const std::string parent_snapshot_id;
    static const std::string parent_volume_id; // unify with parent_nspace_key!
    static const std::string prefetch;
    static const std::string problem;
    static const std::string problems;
    static const std::string queue_count;
    static const std::string queue_size;
    static const std::string restart_local;
    static const std::string reset;
    static const std::string sco_cache_hits;
    static const std::string sco_cache_misses;
    static const std::string sco_multiplier;
    static const std::string sco_size;
    static const std::string scrubbing_name;
    static const std::string scrubbing_work_result;
    static const std::string show_defaults;
    static const std::string size;
    static const std::string snapshot_id;
    static const std::string snapshots;
    static const std::string snapshot_sco_count;
    static const std::string source_data;
    static const std::string start_snapshot;
    static const std::string state;
    static const std::string stored;
    static const std::string success;
    static const std::string target_path;
    static const std::string src_path;
    static const std::string timestamp;
    static const std::string tlog_multiplier;
    static const std::string tlog_name;
    static const std::string tlog_used;
    static const std::string updates;
    static const std::string used;
    static const std::string uuid;
    static const std::string volumedriver;
    static const std::string volumedriver_readonly;
    static const std::string volume_id;
    static const std::string volume_name; // unify with volume_id_key!
    static const std::string volume_size;
    static const std::string vrouter_id;
    static const std::string vrouter_cluster_id;
    static const std::string write_time;
    static const std::string xmlrpc_redirect_host;
    static const std::string xmlrpc_redirect_port;
    static const std::string xmlrpc_error_code;
    static const std::string xmlrpc_error_string;
    static const std::string volume_potential;
    static const std::string timeout;
    static const std::string flags;
};

}

#endif // !VFS_XMLRPC_KEYS_H_
