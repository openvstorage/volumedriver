# Fixing a Transport Endpoint Not Connected
In the event of a unexpected event that crashes the volumedriver, it is possible that the volumedriver mount point is still mounted to an old fuse reference.
In the case of this occuring you will see the following problems in the log files or when performing an `ls` on your volumedriver mountpoint: `Transport Endpoint Not Connected`

## Why
During an unexpected event it is possible that your fuse mountpoint is not properly unmounted. In this case you will have to perform a manual intervention.
This can occur during the following known events:

* Shutting down a volumedriver while processes still use the mountpoint
* The volumedriver being killed by another process/action
* Volumedriver crash

## Fixing the problem
### Shutting down the volumedriver
Shut down the respective volumedriver (if it's still up)

### Cleaning up processes
It is possible that other processes still use the fuse mountpoint, 
in this case you will have to track them down with `lsof` (or other tools) and kill them.

### Unmounting the volumedriver mountpoint
* Currently you will still see the fuse reference being connected to the mountpoint, this can be seen through `mount`.
* Perform a normal `umount` on the mountpoint, e.g. `umount /mnt/myvpool`. If this would fail, at your own risk perform a lazy unmount, e.g. `umount -l /mnt/myvpool`

