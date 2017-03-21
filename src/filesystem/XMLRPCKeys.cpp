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

#include "XMLRPCKeys.h"

// WATCH OUT: This is part of the API.
// If you change the definitions here, the C++ tests will *not* notice as they use
// these XMLRPCKeys as well!

namespace volumedriverfs
{
#define DEFINE_XMLRPC_KEY(name) \
        const std::string XMLRPCKeys::name(#name);

DEFINE_XMLRPC_KEY(backend_config_str);
DEFINE_XMLRPC_KEY(backend_size);
DEFINE_XMLRPC_KEY(backend_type);
DEFINE_XMLRPC_KEY(choking);
DEFINE_XMLRPC_KEY(cleanup_on_success);
DEFINE_XMLRPC_KEY(cleanup_on_error);
DEFINE_XMLRPC_KEY(cluster_cache_behaviour);
DEFINE_XMLRPC_KEY(cluster_cache_handle);
DEFINE_XMLRPC_KEY(cluster_cache_hits);
DEFINE_XMLRPC_KEY(cluster_cache_limit);
DEFINE_XMLRPC_KEY(cluster_cache_misses);
DEFINE_XMLRPC_KEY(cluster_cache_mode);
DEFINE_XMLRPC_KEY(cluster_exponent);
DEFINE_XMLRPC_KEY(cluster_multiplier);
DEFINE_XMLRPC_KEY(cluster_size);
DEFINE_XMLRPC_KEY(code);
DEFINE_XMLRPC_KEY(component_name);
DEFINE_XMLRPC_KEY(configuration);
DEFINE_XMLRPC_KEY(configuration_path);
DEFINE_XMLRPC_KEY(current_sco_count);
DEFINE_XMLRPC_KEY(data_store_read_used);
DEFINE_XMLRPC_KEY(data_store_write_used);
DEFINE_XMLRPC_KEY(delay);
DEFINE_XMLRPC_KEY(delete_global_metadata);
DEFINE_XMLRPC_KEY(delete_local_data);
DEFINE_XMLRPC_KEY(device_info);
DEFINE_XMLRPC_KEY(disposable);
DEFINE_XMLRPC_KEY(end_snapshot);
DEFINE_XMLRPC_KEY(entries);
DEFINE_XMLRPC_KEY(errors);
DEFINE_XMLRPC_KEY(error_string);
DEFINE_XMLRPC_KEY(failover_ip);
DEFINE_XMLRPC_KEY(failover_mode);
DEFINE_XMLRPC_KEY(failover_port);
DEFINE_XMLRPC_KEY(file_name);
DEFINE_XMLRPC_KEY(foc_config);
DEFINE_XMLRPC_KEY(foc_config_mode);
DEFINE_XMLRPC_KEY(footprint);
DEFINE_XMLRPC_KEY(force);
DEFINE_XMLRPC_KEY(free);
DEFINE_XMLRPC_KEY(halted);
DEFINE_XMLRPC_KEY(hits);
DEFINE_XMLRPC_KEY(ignore_foc_if_unreachable);
DEFINE_XMLRPC_KEY(lba_count);
DEFINE_XMLRPC_KEY(lba_size);
DEFINE_XMLRPC_KEY(local_path);
DEFINE_XMLRPC_KEY(log_filter_level);
DEFINE_XMLRPC_KEY(log_filter_name);
DEFINE_XMLRPC_KEY(map_stats);
DEFINE_XMLRPC_KEY(max_size);
DEFINE_XMLRPC_KEY(max_non_disposable_factor);
DEFINE_XMLRPC_KEY(max_non_disposable_size);
DEFINE_XMLRPC_KEY(metadata);
DEFINE_XMLRPC_KEY(metadata_backend_config);
DEFINE_XMLRPC_KEY(metadata_cache_capacity);
DEFINE_XMLRPC_KEY(metadata_store_hits);
DEFINE_XMLRPC_KEY(metadata_store_misses);
DEFINE_XMLRPC_KEY(migrate_cache_to_parent);
DEFINE_XMLRPC_KEY(min_size);
DEFINE_XMLRPC_KEY(misc_info);
DEFINE_XMLRPC_KEY(misses);
DEFINE_XMLRPC_KEY(nspace);
DEFINE_XMLRPC_KEY(new_value);
DEFINE_XMLRPC_KEY(non_disposable);
DEFINE_XMLRPC_KEY(no_updates);
DEFINE_XMLRPC_KEY(object_id);
DEFINE_XMLRPC_KEY(offlined);
DEFINE_XMLRPC_KEY(old_value);
DEFINE_XMLRPC_KEY(param_name);
DEFINE_XMLRPC_KEY(parent_nspace);
DEFINE_XMLRPC_KEY(parent_snapshot_id);
DEFINE_XMLRPC_KEY(parent_volume_id); // unify with parent_nspace!
DEFINE_XMLRPC_KEY(path);
DEFINE_XMLRPC_KEY(prefetch);
DEFINE_XMLRPC_KEY(problem);
DEFINE_XMLRPC_KEY(problems);
DEFINE_XMLRPC_KEY(queue_count);
DEFINE_XMLRPC_KEY(queue_size);
DEFINE_XMLRPC_KEY(redirect_fenced);
DEFINE_XMLRPC_KEY(restart_local);
DEFINE_XMLRPC_KEY(reset);
DEFINE_XMLRPC_KEY(sco_cache_hits);
DEFINE_XMLRPC_KEY(sco_cache_misses);
DEFINE_XMLRPC_KEY(sco_multiplier);
DEFINE_XMLRPC_KEY(sco_size);
DEFINE_XMLRPC_KEY(scrub_manager_counters);
DEFINE_XMLRPC_KEY(scrubbing_name);
DEFINE_XMLRPC_KEY(show_defaults);
DEFINE_XMLRPC_KEY(size);
DEFINE_XMLRPC_KEY(snapshot_id);
DEFINE_XMLRPC_KEY(snapshots);
DEFINE_XMLRPC_KEY(snapshot_sco_count);
DEFINE_XMLRPC_KEY(source_data);
DEFINE_XMLRPC_KEY(start_snapshot);
DEFINE_XMLRPC_KEY(state);
DEFINE_XMLRPC_KEY(stored);
DEFINE_XMLRPC_KEY(success);
DEFINE_XMLRPC_KEY(src_path);
DEFINE_XMLRPC_KEY(target_path);
DEFINE_XMLRPC_KEY(timestamp);
DEFINE_XMLRPC_KEY(tlog_multiplier);
DEFINE_XMLRPC_KEY(tlog_name);
DEFINE_XMLRPC_KEY(tlog_used);
DEFINE_XMLRPC_KEY(updates);
DEFINE_XMLRPC_KEY(used);
DEFINE_XMLRPC_KEY(uuid);
DEFINE_XMLRPC_KEY(volumedriver);
DEFINE_XMLRPC_KEY(volumedriver_readonly);
DEFINE_XMLRPC_KEY(volume_id);
DEFINE_XMLRPC_KEY(volume_name); // unify with volume_id!
DEFINE_XMLRPC_KEY(volume_size);
DEFINE_XMLRPC_KEY(vrouter_id);
DEFINE_XMLRPC_KEY(vrouter_cluster_id);
DEFINE_XMLRPC_KEY(write_time);
DEFINE_XMLRPC_KEY(xmlrpc_redirect_host);
DEFINE_XMLRPC_KEY(xmlrpc_redirect_port);
DEFINE_XMLRPC_KEY(scrubbing_work_result);
DEFINE_XMLRPC_KEY(xmlrpc_error_code);
DEFINE_XMLRPC_KEY(xmlrpc_error_string);
DEFINE_XMLRPC_KEY(volume_potential);
DEFINE_XMLRPC_KEY(number_of_syncs_to_ignore);
DEFINE_XMLRPC_KEY(maximum_time_to_ignore_syncs_in_seconds);
DEFINE_XMLRPC_KEY(timeout);
DEFINE_XMLRPC_KEY(flags);

#undef DEFINE_XMLRPC_KEY

}
