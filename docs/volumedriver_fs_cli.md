## volumedriver_fs
The VolumeDriver File System can be managed through a command line interface from any of the nodes. To execute a command run

```
volumedriver_fs <command>
```

To get an overview of all possible options for the file system
```
volumedriver_fs --help
```

#### The available options
Logging Options:
```
  --logfile arg              Where to write the logging statements, if empty
                             log to console
  --loglevel arg (=info)     Logging level, one of trace, debug, info, warning,
                             error, fatal
  --disable-logging          Whether to disable the logging
  --logrotation              whether to rotate the file at midnight
  --logfilter arg            a log filter, each filter is of the form
                             'logger_name loggerlevel', multiple filters can be
                             added
```
General Options:
```
  -h [ --help ]              produce help message on cout and exit
  -v [ --version ]           print version and exit
```

Backend Options:
```
  --backend-config-file arg  backend config file, a json file that holds the
```                             backend configuration

FileSystem options:
```
  -C [ --config-file ] arg volumedriver (json) config file
  -L [ --lock-file ] arg   a lock file used for advisory locking to prevent
                           concurrently starting the same instance - the
                           config-file is used if this is not specified
  -M [ --mountpoint ] arg  path to mountpoint
```

FUSE options:
```
    -d   -o debug          enable debug output (implies -f)
    -f                     foreground operation
    -s                     disable multi-threaded operation
    -o allow_other         allow access to other users
    -o allow_root          allow access to root
    -o auto_unmount        auto unmount on process termination
    -o nonempty            allow mounts over non-empty file/dir
    -o default_permissions enable permission checking by kernel
    -o fsname=NAME         set filesystem name
    -o subtype=NAME        set filesystem type
    -o large_read          issue large read requests (2.4 only)
    -o max_read=N          set maximum size of read requests
    -o hard_remove         immediate removal (don't hide files)
    -o use_ino             let filesystem set inode numbers
    -o readdir_ino         try to fill in d_ino in readdir
    -o direct_io           use direct I/O
    -o kernel_cache        cache files in kernel
    -o [no]auto_cache      enable caching based on modification times (off)
    -o umask=M             set file permissions (octal)
    -o uid=N               set file owner
    -o gid=N               set file group
    -o entry_timeout=T     cache timeout for names (1.0s)
    -o negative_timeout=T  cache timeout for deleted names (0.0s)
    -o attr_timeout=T      cache timeout for attributes (1.0s)
    -o ac_attr_timeout=T   auto cache timeout for attributes (attr_timeout)
    -o noforget            never forget cached inodes
    -o remember=T          remember cached inodes for T seconds (0s)
    -o nopath              don't supply path if not necessary
    -o intr                allow requests to be interrupted
    -o intr_signal=NUM     signal to send on interrupt (10)
    -o modules=M1[:M2...]  names of modules to push onto filesystem stack
    -o max_write=N         set maximum size of write requests
    -o max_readahead=N     set maximum readahead
    -o max_background=N    set number of maximum background requests
    -o congestion_threshold=N  set kernel's congestion threshold
    -o async_read          perform reads asynchronously (default)
    -o sync_read           perform reads synchronously
    -o atomic_o_trunc      enable atomic open+truncate support
    -o big_writes          enable larger than 4kB writes
    -o no_remote_lock      disable remote file locking
    -o no_remote_flock     disable remote file locking (BSD)
    -o no_remote_posix_lock disable remove file locking (POSIX)
    -o [no_]splice_write   use splice to write to the fuse device
    -o [no_]splice_move    move data while splicing to the fuse device
    -o [no_]splice_read    use splice to read from the fuse device
```