# VolumeDriver Config file
The VolumeDriver gets configured per vPool. To change the config file, edit the json. Some of the values can be dynamically changed (dynamic reconfigurable), other values require a restart. To make sure the static values get applied execute
```
service ovs-volumedriver-vpool_name restart
```

**Note: Modifying the config files incorrectly might result in data loss.**

| component | key | default value | dynamically reconfigurable | remarks |
| --- | --- | --- | --- | --- |
| volume_router | vrouter_local_io_sleep_before_retry_usecs | "100000" | yes | delay (microseconds) before rerrying a request that failed with a retryable error |
| volume_router | vrouter_local_io_retries | "600" | yes | number of retry attempts for requests that failed with a retryable error |
| volume_router | vrouter_check_local_volume_potential_period | "1" | yes | how often to recheck the local volume potential during migration |
| volume_router | vrouter_volume_read_threshold | "0" | yes | number of remote read requests before auto-migrating a volume - 0 turns it off |
| volume_router | vrouter_volume_write_threshold | "0" | yes | number of remote write requests before auto-migrating a volume - 0 turns it off |
| volume_router | vrouter_file_read_threshold | "0" | yes | number of remote read requests before auto-migrating a file - 0 turns it off |
| volume_router | vrouter_file_write_threshold | "0" | yes | number of remote write requests before auto-migrating a file - 0 turns it off |
| volume_router | vrouter_redirect_timeout_ms | "0" | yes | timeout for redirected requests in milliseconds - 0 turns it off |
| volume_router | vrouter_backend_sync_timeout_ms | "0" | yes | timeout for remote backend syncs (during migration) - 0 turns it off |
| volume_router | vrouter_migrate_timeout_ms | "500" | yes | timeout for migration requests in milliseconds (in addition to remote backend sync timeout!) |
| volume_router | vrouter_redirect_retries | "10" | yes | number of retries to after a redirect timed out |
| volume_router | vrouter_id | --- | no | the vrouter_id of this node of the cluster. Must be one of the vrouter_id's s specified in the vrouter_cluster_nodes section |
| volume_router | vrouter_sco_multiplier | "1024" | no | number of clusters in a sco |
| volume_router | vrouter_routing_retries | "30" | yes | number of times the routing shall be retried in case the volume is not found (exponential backoff inbetween retries!) |
| volume_router | vrouter_min_workers | "4" | yes | minimum number of worker threads to handle redirected requests |
| volume_router | vrouter_max_workers | "16" | yes | maximum number of worker threads to handle redirected requests |
| volume_router | vrouter_registry_cache_capacity | "1024" | no | number of ObjectRegistrations to keep cached |
| volume_router | vrouter_use_fencing | "0" | yes | whether to use fencing support if it is available |
| volume_router | vrouter_send_sync_response | "0" | yes | whether to send extended response data on sync requests |
| volume_router_cluster | vrouter_cluster_id | --- | no | cluster_id of the volumeroutercluster this node belongs to |
| fuse | fuse_min_workers | "8" | yes | minimum number of FUSE worker threads |
| fuse | fuse_max_workers | "8" | yes | maximum number of FUSE worker threads |
| shm_interface | shm_region_size | "268435456" | no | size in bytes of the shared memory segment |
| network_interface | network_uri | "tcp://127.0.0.1:21321" | no | URI to bind network interface |
| network_interface | network_snd_rcv_queue_depth | "2048" | no | Maximum tx/rx queued messages |
| network_interface | network_workqueue_max_threads | "4" | no | Maximum workqueue threads |
| network_interface | network_workqueue_ctrl_max_threads | "128" | no | Maximum control path workqueue threads |
| network_interface | network_max_neighbour_distance | "4294967295" | yes | Hide nodes that have a distance >= this value from network clients |
| filesystem | fs_ignore_sync | "0" | yes | ignore sync requests - AT THE POTENTIAL EXPENSE OF DATA LOSS |
| filesystem | fs_virtual_disk_format | --- | no | virtual disk format: vmdk or raw |
| filesystem | fs_raw_disk_suffix | "" | no | Suffix to use when creating clones if fs_virtual_disk_format=raw |
| filesystem | fs_max_open_files | "65536" | no | Maximum number of open files, is set using rlimit() on startup |
| filesystem | fs_file_event_rules | "[]" | no | an array of filesystem event rules, each consisting of a "path_regex" and an array of "fs_calls" |
| filesystem | fs_metadata_backend_type | "MDS" | no | Type of metadata backend to use for volumes created via the filesystem interface |
| filesystem | fs_metadata_backend_arakoon_cluster_id | "" | no | Arakoon cluster identifier for the volume metadata |
| filesystem | fs_metadata_backend_arakoon_cluster_nodes | "[]" | no | an array of arakoon cluster node configurations for the volume metadata, each containing node_id, host and port |
| filesystem | fs_metadata_backend_mds_nodes | "[]" | yes | an array of MDS node configurations for the volume metadata, each containing host and port |
| filesystem | fs_metadata_backend_mds_apply_relocations_to_slaves | "1" | yes | an bool indicating whether to apply relocations to slave MDS tables |
| filesystem | fs_metadata_backend_mds_timeout_secs | "20" | yes | timeout (in seconds) for calls to MDS servers |
| filesystem | fs_cache_dentries | "0" | no | whether to cache directory entries locally |
| filesystem | fs_dtl_config_mode | "Automatic" | no | Configuration mode : Automatic | Manual |
| filesystem | fs_dtl_host | "" | yes | DTL host |
| filesystem | fs_dtl_port | "0" | yes | DTL port |
| filesystem | fs_dtl_mode | "Asynchronous" | yes | DTL mode : Asynchronous | Synchronous |
| filesystem | fs_enable_shm_interface | "0" | no | Whether to enable the SHM interface |
| filesystem | fs_enable_network_interface | "0" | no | Whether to enable the network interface |
| event_publisher | events_amqp_uris | "[]" | yes | array of URIs, each consisting of an "amqp_uri" entry for a node of the AMQP cluster events shall be sent to |
| event_publisher | events_amqp_exchange | "" | yes | AMQP exchange events will be sent to |
| event_publisher | events_amqp_routing_key | "" | yes | AMQP routing key used for sending events |
| distributed_transaction_log | dtl_path | --- | no | path to the directory the DTL writes its data in |
| distributed_transaction_log | dtl_transport | "TCP" | no | transport to use for the DTL (RDMA|TCP) |
| volume_registry | vregistry_arakoon_timeout_ms | "60000" | yes | Arakoon client timeout in milliseconds for the volume registry |
| volume_registry | vregistry_arakoon_cluster_id | --- | no | Arakoon cluster identifier for the volume registry |
| volume_registry | vregistry_arakoon_cluster_nodes | --- | yes | an array of arakoon cluster node configurations for the volume registry, each containing node_id, host and port |
| scrub_manager | scrub_manager_interval | "300" | yes | interval (in seconds) of the ScrubManager |
| scrub_manager | scrub_manager_sync_wait_secs | "600" | yes | number of seconds to wait for a scrub result application to be on the backend before giving up |
| stats_collector | stats_collector_interval_secs | "6000" | yes | interval with which stats and counters are pushed |
| stats_collector | stats_collector_destination | "" | yes | destination of stats and counters |
| file_driver | fd_cache_path | --- | no | cache for filedriver objects |
| file_driver | fd_namespace | --- | no | backend namespace to use for filedriver objects |
| file_driver | fd_extent_cache_capacity | "1024" | no | number of extents the extent cache can hold |
| threadpool_component | num_threads | "4" | yes | Number of threads writing SCOs to the backend |
| volume_manager | metadata_path | --- | no | Directory, where to create subdirectories in for volume metadata storage |
| volume_manager | tlog_path | --- | no | Directory, where to create subdirectories for volume tlogs |
| volume_manager | open_scos_per_volume | "32" | no | Number of open SCOs per volume |
| volume_manager | dtl_throttle_usecs | "1000" | yes | Timeout for retrying writes to the DTL |
| volume_manager | dtl_queue_depth | "1024" | yes | Size of the queue of entries to be sent to the DTL |
| volume_manager | dtl_write_trigger | "8" | yes | Trigger to start writing entries in the foc queue to the backend |
| volume_manager | dtl_busy_loop_usecs | "0" | yes | Timeout for busy retries sending/reading data before falling back to polling |
| volume_manager | dtl_request_timeout_ms | "60000" | yes | Timeout for DTL requests |
| volume_manager | dtl_connect_timeout_ms | "1000" | yes | Timeout for connection attempts to the DTL - 0: wait forever / the OS to signal errors |
| volume_manager | clean_interval | --- | yes | Interval between runs of scocache cleanups, in seconds. Should be small when running on ramdisk, larger when running on sata. scocache_cleanup_trigger / clean_interval should be larger than the aggregated write speed to the scocache. |
| volume_manager | sap_persist_interval | "300" | yes | Interval between writing SAP data, in seconds |
| volume_manager | dtl_check_interval_in_seconds | "300" | yes | Interval between checks of the DTL state of volumes |
| volume_manager | read_cache_default_behaviour | "CacheOnRead" | yes | Default read cache behaviour, should be CacheOnWrite, CacheOnRead or NoCache |
| volume_manager | read_cache_default_mode | "ContentBased" | yes | Default read cache mode, should be ContentBased or LocationBased |
| volume_manager | sco_written_to_backend_action | "SetDisposable" | yes | Default SCO cache behaviour (SetDisposable, SetDisposableAndPurgeFromPageCache, PurgeFromSCOCache) |
| volume_manager | required_tlog_freespace | "209715200" | yes | Required free space in the tlog directory |
| volume_manager | required_meta_freespace | "209715200" | yes | Required free space in the metadata directory |
| volume_manager | freespace_check_interval | "10" | yes | Interval between checks of required freespace parameters, in seconds |
| volume_manager | number_of_scos_in_tlog | "20" | no | The number of SCOs that trigger a tlog rollover |
| volume_manager | non_disposable_scos_factor | "1.5" | no | Factor to multiply number_of_scos_in_tlog with to determine the amount of non-disposable data permitted per volume |
| volume_manager | default_cluster_size | "4096" | no | size of a cluster in bytes |
| volume_manager | metadata_cache_capacity | "8192" | no | number of metadata pages to keep cached |
| volume_manager | debug_metadata_path | "/opt/OpenvStorage/var/lib/volumedriver/evidence" | no | place to store evidence when a volume is halted. |
| volume_manager | arakoon_metadata_sequence_size | "10" | no | Size of Arakoon sequences used to send metadata pages to Arakoon |
| scocache | trigger_gap | --- | no | scocache-mountpoint freespace threshold below which scocache-cleaner is triggered |
| scocache | backoff_gap | --- | no | scocache-mountpoint freespace objective for scocache-cleaner |
| scocache | scocache_mount_points | --- | no | An array of directories and sizes to be used as scocache mount points |
| content_addressed_cache | read_cache_serialization_path | --- | no | Directory to store the serialization of the Read Cache |
| content_addressed_cache | serialize_read_cache | "1" | no | Whether to serialize the readcache on exit or not |
| content_addressed_cache | clustercache_mount_points | "[]" | no | An array of directories and sizes to be used as Read Cache mount points |
| distributed_lock_store | dls_type | "Backend" | no | Type of distributed lock store to use (default / currently only supported value: "Backend") |
| distributed_lock_store | dls_arakoon_timeout_ms | "60000" | yes | Arakoon client timeout in milliseconds for the distributed lock store |
| distributed_lock_store | dls_arakoon_cluster_id | "" | no | Arakoon cluster identifier for the distributed lock store |
| distributed_lock_store | dls_arakoon_cluster_nodes | "[]" | yes | an array of arakoon cluster node configurations for the distributed lock store, each containing node_id, host and port |
| metadata_server | mds_db_type | "ROCKSDB" | no | Type of database to use for metadata. Supported values: ROCKSDB |
| metadata_server | mds_cached_pages | "256" | no | Capacity of the metadata page cache per volume |
| metadata_server | mds_poll_secs | "300" | yes | Poll interval for the backend check in seconds |
| metadata_server | mds_timeout_secs | "30" | no | Timeout for network transfers - (0 -> no timeout!) |
| metadata_server | mds_threads | "1" | no | Number of threads per node (0 -> autoconfiguration based on the number of available CPUs) |
| metadata_server | mds_nodes | "[]" | yes | an array of MDS node configurations each containing address, port, db_directory and scratch_directory |
| backend_connection_manager | backend_connection_pool_capacity | "64" | yes | Capacity of the connection pool maintained by the BackendConnectionManager |
| backend_connection_manager | backend_connection_pool_shards | "1" | no | Number of shards for the connection pool, 0 -> one per CPU |
| backend_connection_manager | backend_interface_retries_on_error | "1" | yes | How many times to retry a failed backend operation |
| backend_connection_manager | backend_interface_retry_interval_secs | "0" | yes | delay before retrying a failed backend operation in seconds |
| backend_connection_manager | backend_interface_retry_backoff_multiplier | "1" | yes | multiplier for the retry interval on each subsequent retry |
| backend_connection_manager | backend_type | "LOCAL" | no | Type of backend connection one of ALBA, LOCAL, or S3, the other parameters in this section are only used when their correct backendtype is set |
| backend_connection_manager | local_connection_path | --- | no | When backend_type is LOCAL: path to use as LOCAL backend, otherwise ignored |
| backend_connection_manager | s3_connection_host | "s3.amazonaws.com" | no | When backend_type is S3: the S3 host to connect to, otherwise ignored |
| backend_connection_manager | s3_connection_port | "80" | no | When backend_type is S3: the S3 port to connect to, otherwise ignored |
| backend_connection_manager | s3_connection_username | "" | no | When backend_type is S3: the S3 username, otherwise ignored |
| backend_connection_manager | s3_connection_password | "" | no | When backend_type is S3: the S3 password |
| backend_connection_manager | s3_connection_verbose_logging | "1" | no | When backend_type is S3: whether to do verbose logging |
| backend_connection_manager | s3_connection_use_ssl | "0" | no | When backend_type is S3: whether to use SSL to encrypt the connection |
| backend_connection_manager | s3_connection_ssl_verify_host | "1" | no | When backend_type is S3: whether to verify the SSL certificate's subject against the host |
| backend_connection_manager | s3_connection_ssl_cert_file | "" | no | When backend_type is S3: path to a file holding the SSL certificate |
| backend_connection_manager | s3_connection_flavour | "S3" | no | S3 backend flavour: S3 (default), GCS, WALRUS or SWIFT |
| backend_connection_manager | alba_connection_host | "127.0.0.1" | no | When backend_type is ALBA: the ALBA host to connect to, otherwise ignored |
| backend_connection_manager | alba_connection_port | "56789" | no | When the backend_type is ALBA: The ALBA port to connect to, otherwise ignored |
| backend_connection_manager | alba_connection_timeout | "5" | no | The timeout for the ALBA proxy, in seconds |
| backend_connection_manager | alba_connection_preset | "" | no | When backend_type is ALBA: the ALBA preset to use for new namespaces |
| backend_connection_manager | alba_connection_transport | "TCP" | no | When backend_type is ALBA: the ALBA connection to use: TCP (default) or RDMA |
| backend_connection_manager | alba_connection_use_rora | "0" | no | Whether to enable Read Optimized RDMA ASD (RORA) support |
| backend_connection_manager | alba_connection_rora_manifest_cache_capacity | "10000" | no | Capacity of the RORA fetcher's manifest cache |
| backend_garbage_collector | bgc_threads | "4" | yes | Number of threads employed by the BackendGarbageCollector |

In case a dynamic property is changed, notify the Volume Driver of the update with the python api.

```
import volumedriver.storagerouter.storagerouterclient as src
lclient = src.LocalStorageRouterClient("/opt/OpenvStorage/config/storagedriver/storagedriver/<vpool_name>.json")
lclient.update_configuration("/opt/OpenvStorage/config/storagedriver/storagedriver/<vpool_name>.json")
```
** Note: update the json of all nodes of the vPool to make sure the vPool is consistent**
