<a name="shm"></a>
## Shared Memory Server
A Shared Memory Client/Server implementation is used to give Open vStorage the best performance possible. With this implementation the client (QEMU, Blktab, ...)
can write to a dedicated memory segment on the compute host which is shared with the Shared Memory Server in the Volume Driver.
The segment is mapped as device under `/dev/shm` and has by default a size of 256 MB. The size can in the vPool json under `shm_region_size`. 
Next to this segment there are 4 shared memory message queues per volumes. These queues (read request, read reply, write request and write reply) are also shared
between the client and the server . These queues are used to signal incoming requests and signal when the request is executed.

The Shared Memory Server implementation can work synchronous or asynchronous if the client supports it. Processing the IO requests asynchronously generally leads to better performance.

Currently the Shared Memory Server supports 2 interfaces: QEMU and block devices (Blktap).

Details about the Open vStorage VolumeDriver Shared Memory Access C library can be found [here](https://github.com/openvstorage/volumedriver/blob/dev/doc/libovsvolumedriver.txt).
The SHM API can be found under `/usr/lib/openvstorage/libovsvolumedriver.so`.
The header file can be found under `/usr/include/openvstorage/volumedriver.sh`.
