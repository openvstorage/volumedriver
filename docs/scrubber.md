<a name="scrubber"></a>
## Scrubber
A log structured time based storage system results in a certain set of data becoming invalid due to overwrites. This is a rather bold statement
and is not always true of course. To give one simple example: for instance when snapshots were taken in between overwrites than the different
versions are not obsoleting each other.

Details on how the scrubber works and its required storage resources along the process is explained in the [scrubbing process](scrub_process.md)

### Calculate the storage requirements

#### The formula

How much space will be temporarely consumed by a single scrubwork job based on the tlogs_size

    scratch_dir_usage = ( 9 * ( snapshot_backend_size / ( block_size / ( size_of_entry * 2 ) ) ) ) + snapshot_file_size + ( 2 * SCO_size )

#### Example with common values

Further calculations are done with the following values:

- SCO_size = 64 MiB
- cluster or block_size = 4096 bytes
- size_of_entry = 16 bytes (for no-dedupe volumedriver)
- snapshot_file_size = 2 MiB

resulting in the formula becoming

    scratch_dir_size = ( 9 * ( snapshot_backend_size / ( 4096 / ( 16 * 2 ) ) ) ) + 2 MiB + ( 2 * 64 MiB )

or in short

    scratch_dir_size = ( snapshot_backend_size / 14,22 ) + 130 MiB

Turning the formula around allows you to calculate the maximum snapshot_backend_size for a given scratch_dir_size

    snapshot_backend_size = ( scratch_dir_size - 130 MiB ) * 14,22

Suppose a scratch_dir_size of 100 GB = 97,65625 GiB = 100000 MiB, we get to the following

    snapshot_backend_size = ( 100000 MiB - 130MiB ) * 14,22 = 1420151,4 MiB = 1386,866 GiB = 1,354 TiB

