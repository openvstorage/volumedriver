<a name="Scrub Process"></a>
## Scrubbing stages

Input to the scrubbing is

- interface to the backend, and namespace to be scrubbed
- snapshot_to_be_scrubbed
- fill_ratio
- scratch_dir

The scrubbing happens in a dedicated area \-\- the scratch dir. This is completely removed after the scrubbing has finished.
Given that a snapshot has snapshot_backend_size of data in it we can estimate the size of the TLogs for that snapshot as:

    number_of_clusters = snapshot_backend_size / block_size
    tlogs_size = size_of_entry * number_of_clusters

giving after filling in the values for block_size (= 4096) and size_of_entry (= clusteraddress[8] + clusterlocation[8])

    tlogs_size = snapshot_backend_size / 256

This is a slight underestimate since the tlog will also have crc entries for scos and tlogs in them

but the following is a hard limit:

    tlogs_size < snapshot_backend_size / 128


### Metadata Scrub

#### Get the tlogs from the backend

Get the snapshots file from the backend. Get the TLogs in snapshot_to_be_scrubbed.

    scratch_dir_usage = tlogs_size + snapshot_file_size
    backend_usage = 0

#### Split them up in regions

To aid in the scrubbing of large volumes we divide the volume up in regions which we then scrub separately.
This parameter does not play a role for the current volume sizes used in Open vStorage so practically we are using only 1 region:

    scratch_dir_usage = 2 * tlogs_size + snapshot_file_size
    backend_usage = 0

#### Metadata Scrub the separate regions

We walk through the metadata backwards and discard all entries for a lba address we have already seen \-\-meaning it has been overwritten by a later entry.

    scratch_dir_usage = 3 * tlogs_size + snapshot_file_size
    backend_usage = 0

#### Merge the tlogs for the separate regions

The scrubbed regions are merged together again into 1 forwards ordered tlog.

    scratch_usage = 4 * tlogs_size + snapshot_file_size
    backend_usage = 0

### Data Scrub

#### Actual Datascrub

We only scrub the SCO's for which the

     number_of_unused_clusters_in_scos / number_of_clusters_in_scos <= fill_ratio

to avoid doing work of getting SCO's and rewriting them when the storage gains are not worth it.
Every SCO that will be rewritten is retrieved from the backend and it's data is put into a new SCO. The tlog is split up into 2 parts, 
1 holding metadata for rewritten SCO's and 1 holding data for non rewritten SCO's.
Next to that we keep a list of the relocations, these are pairs of TLog entries to know where a cluster has moved to after scrubbing.

    scratch_dir_usage = 7* tlogs_size + snapshot_file_size + 2 * SCO_size
    backend_usage = backend_size

#### Merge of rewritten_tlog and non_rewritten_tlog and splitting them up again in smaller pieces

    scratch_dir_usage = 9 * tlogs_size + snapshot_file_size + 2 * SCO_size
    backend_usage = backend_size

#### Putting the scrubbing result files on the backend

    scratch_dir_usage = 9 * tlogs_size + snapshot_file_size + 2 * SCO_size
    backend_usage = backend_size + 3* tlogs_size

### Applying the Scrubbing

Output of the scrubbing process is:

* A list of the new TLogs
* A list of the old TLogs
* A list of relocations
* A list of SCO_names to be deleted
* A list of new SCO_names

At this point nothing of the volume has been changed yet, no data has been deleted only new data added.