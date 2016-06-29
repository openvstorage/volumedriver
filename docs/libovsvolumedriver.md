libovsvolumedriver(7)
=====================

NAME
----

libovsvolumedriver - Open vStorage VolumeDriver Access C library

SYNOPSIS
--------
*cc* ['flags'] 'files' *-lovsvolumedriver* ['libraries']

DESCRIPTION
-----------

The following functions are exported by libovsvolumedriver library:

Initialize and destroy context attributes object::
    ovs_ctx_attr_t* ovs_ctx_attr_new();
    int ovs_ctx_attr_destroy(ovs_ctx_attr_t *attr);

    Description
    -----------
    The ovs_ctx_attr_new() function creates a context attributes object
    with default attribute values. After this call, individual attributes of
    the object can be set using various related functions, and then the
    object can be used in one or more ovs_ctx_new() calls.

    When a context attribute is no longer required, it should be destroyed
    using the ovs_ctx_attr_destroy() function. Destroying a context attributes
    object has no effect on contexes that were created using that object.

    Once a context attributes object has been destroyed, it can be
    reinitialized using ovs_ctx_attr_new(). Any other use of a destroyed
    context attributes object has undefined results.

    Return Value
    ------------
    On success, these functions return a context attributes object
    respectively or 0; on error, they return NULL or a nonzero error number.

    Errors
    ------
    The function shall fail if:
        - EINVAL Invalid arguments supplied
        - ENOMEM Out of memory

Set transport layer::
    int ovs_ctx_attr_set_transport(ovs_ctx_attr_t *attr,
                                   const char *transport,
                                   const char *host,
                                   int port);

    Description
    -----------
    The ovs_ctx_attr_set_transport() function sets the underlying transport
    layer. After this call, individual attributes of the object are set that
    can be used in one or more ovs_ctx_new() calls.

    The 'transport' string specifies the transport used to connect to the
    volumedriver daemon. Specifying NULL will result in the usage of the
    default shared memory server. Permitted values are those what you specify
    as transport-type in a volume specification e.g, "tcp", "rdma" and "shm".

    The 'host' string specifies the address where to find the volumedriver
    daemon. This would either be
        - FQDN (e.g : "storage01.openvstorage.com") or
        - ASCII (e.g : "192.168.1.2")

    The 'port' value specifies the TCP/RDMA port number where volumedriver
    daemon is listening. Specifying 0 uses the default port number
    OVS_DEFAULT_BASE_PORT. This parameter is unused if you are using shared
    memory.

    Return Value
    ------------
    On success 0 is returned, otherwise it shall return -1 and errno will be
    set with the type of failure.

    Errors
    ------
    The function shall fail if:
        - EINVAL Invalid arguments supplied

Set network queue depth::
    int ovs_ctx_attr_set_network_qdepth(ovs_ctx_attr_t *attr,
                                        const uint64_t qdepth);

    Description
    -----------
    The ovs_ctx_attr_set_network_qdepth() function sets the underlying network
    queue depth. After this call, individual attributes of the object are set
    that can be used in one or more ovs_ctx_new() calls. The default value that
    is used when the attributes object is created is 256.

    Return Value
    ------------
    On success 0 is returned, otherwise it shall return -1 and errno will be
    set with the type of failure.

    Errors
    ------
    The function shall fail if:
        - EINVAL Invalid arguments supplied

Create Open vStorage context::
    ovs_ctx_t *ovs_ctx_new(const ovs_ctx_attr_t *attr);

    Description
    -----------
    The ovs_ctx_new() function creates a new Open vStorage context. This is
    the most likely the very first function that should be used. The ovs_ctx_t
    object should be initialized with ovs_ctx_init() before it can be used in
    volume I/O operations.

    The 'attr' context attributes object will properly initialize the context
    object.

    Return Value
    ------------
    Upon successful completion the function shall return an Open vStorage
    context. Otherwise it shall return NULL and errno will indicate the error.

    Errors
    ------
    The function shall fail if:
        - EINVAL Invalid arguments supplied
        - ENOMEM Out of memory

Initialize Open vStorage context::
    int ovs_ctx_init(ovs_ctx_t* ctx, const char *volname, int oflag);

    Description
    -----------
    The ovs_ctx_init() shall establish the connection between a volume and
    a context.

    Values for 'oflag' are constructed by a bitwise-inclusive OR of flags.
    Applications shall specify exactly one of the first three values
    (volume access modes) below in the value of 'oflag':

        - O_RDONLY Open for reading
        - O_WRONLY Open for writing
        - O_RDWR   Open for reading and writing

    Return Value
    ------------
    Upon successful completion the function shall return 0. Otherwise it shall
    return -1 and errno will indicate the error.

    Errors
    ------
    The function shall fail if:
        - EACCES The requested access to the volume is not allowed, or the
          volume did not exist yet or write access to the parent
          directory is not allowed.
        - EINVAL Invalid arguments supplied
        - EBUSY The volume is already open by another process
        - EIO An error occurred during context creation

Terminate Open vStorage context::
    int ovs_ctx_destroy(ovs_ctx_t *ctx);

    Description
    -----------
    The ovs_ctx_destroy() shall destroy an Open vStorage context

    Return Value
    ------------
    Upon successful completion the function shall return 0 otherwise -1 is
    returned and errno will indicate the error.

    Errors
    ------
    The function shall fail if:
        - EINVAL Invalid argument supplied

Create a new volume::
    int ovs_create_volume(ovs_ctx_t *ctx, const char *volname,
                          uint64_t size);

    Description
    -----------
    The ovs_create_volume() shall create a new volume of size 'size' and
    name 'volname'.

    Return Value
    ------------
    Upon successful completion the function shall create a new
    volume and shall return 0 if successful otherwise -1 is returned, no
    volume shall be created.

    Errors
    ------
    The function shall fail if:
        - EEXIST The named volume exists
        - EINVAL Invalid arguments supplied
        - EIO An error occured during ovs_create_volume()
        - ENOSPC The backend that would contain the new volume cannot be
          expanded

Remove a volume::
    int ovs_remove_volume(ovs_ctx_t *ctx, const char *volname);

    Description
    -----------
    The ovs_remove_volume() shall remove a volume of name 'volname'.

    Return Value
    ------------
    Upon successful completion the function shall remove the volume and shall
    return 0 if successful otherwise -1 is returned, no volume shall be
    removed.

    Errors
    ------
    The function shall fail if:
        - ENOENT The named volume doesn't exist
        - EINVAL Invalid arguments supplied
        - EIO An error occured during ovs_remove_volume()

Truncate a volume to a specified length::
    int ovs_truncate_volume(ovs_ctx_t *ctx, const char *volname,
                            uint64_t length);

    Description
    -----------
    The ovs_truncate_volume() shall truncate an already created volume of
    name 'volname' to a size of precisely 'length' bytes.

    Return Value
    ------------
    Upon successful completion the function shall truncate the volume and shall
    return 0 if successful otherwise -1 is returned, no volume shall be
    truncated.

    Errors
    ------
    The function shall fail if:
        - ENOENT The named volume doesn't exist
        - EINVAL Invalid arguments supplied
        - EIO An error occured during ovs_truncate_volume()

Snapshot a volume
    int ovs_snapshot_create(ovs_ctx_t *ctx, const char *volname,
                            const char *snapname, const int64_t timeout);

    Description
    -----------
    The ovs_snapshot_create() shall create a snapshot of name 'snapname' of
    a volume of name 'volname'.
    If timeout is a positive number the function shall wait for the snapshot
    to be synced on backend, until the timeout expires. If timeout value is
    zero the function shall wait until the snapshot is synced on the backend.
    For negative timeout values the function shall return immediately.

    Return Value
    ------------
    Upon successful completion the function shall create a new snapshot and
    shall return 0 if successful otherwise -1 is returned, no snapshot shall
    be created.

    Errors
    ------
    The function shall fail if:
        - ENOENT The named volume doesn't exist
        - EINVAL Invalid arguments supplied
        - EEXIST The snapshot with name 'snapname' already exists
        - ETIMEDOUT Timeout exceeded, snapshot failed
        - EBUSY Previous snapshot not on backend yet, cannot take a snapshot
        - EIO An error occured during ovs_snapshot_create()

Rollback volume to a specific snapshot
    int ovs_snapshot_rollback(ovs_ctx_t *ctx, const char *volname,
                              const char *snapname);

    Description
    -----------
    The ovs_snapshot_rollback() shall rollback the volume of name 'volname' to
    the snapshot of name 'snapname'.

    Return Value
    ------------
    Upon successful completion the function shall rollback volume to the
    specified snapshot and shall return 0 if successful otherwise -1 is
    returned.

    Errors
    ------
    The function shall fail if:
        - ENOENT The named volume or snapshot doesn't exist
        - EINVAL Invalid arguments supplied
        - ENOTEMPTY The snapshot has children
        - EIO An error occured during ovs_snapshot_rollback()

Remove snapshot
    int ovs_snapshot_remove(ovs_ctx_t *ctx, const char *volname,
                            const char *snapname);

    Description
    -----------
    The ovs_snapshot_remove() shall remove the snapshot of name 'snapname'
    from volume of name 'volname'.

    Return Value
    ------------
    Upon successful completion the function shall rollback volume to the
    specified snapshot and shall return 0 if successful otherwise -1 is
    returned.

    Errors
    ------
    The function shall fail if:
        - ENOENT The named volume or snapshot doesn't exist
        - EINVAL Invalid arguments supplied
        - ENOTEMPTY The volume has children
        - EIO An error occured during ovs_snapshot_remove()

List snapshots of a volume
    int ovs_snapshot_list(ovs_ctx_t *ctx, const char *volname,
                          ovs_snapshot_info_t *snap_list, int *max_snaps);

    Description
    -----------
    The ovs_snapshot_list() shall return the snapshots for the volume of
    name 'volname'. The value of 'max_snaps' denotes the size of 'snap_list'.
    If the number of snapshots is larger than 'max_snaps' then the
    aforementioned variable will be populated with the number of snapshots.

    Return Value
    ------------
    Upon successful completion the function shall return the number of
    snapshots for the specific volume otherwise -1 is returned.

    Errors
    ------
    The function shall fail if:
        - ENOENT The named volume or snapshot doesn't exist
        - EINVAL Invalid arguments supplied
        - ENOMEM Out of memory
        - ERANGE The size of 'snap_list' is smaller than the number of
          snapshots. The 'max_snaps' variable is populated with the number of
          snapshots
        - EIO An error occured during ovs_snapshot_list()

Free snapshots list
    void ovs_snapshot_list_free(ovs_snapshot_info_t *snap_list)

    Description
    -----------
    The ovs_snapshot_list_free() shall free the snapshot list.

    Return Value
    ------------
    No return value.

    Errors
    ------
    No errors.

Check if snapshot is synced on the backend
    int ovs_snapshot_is_synced(ovs_ctx_t *ctx,
                               const char *volname,
                               const char *snapshot_name);

    Description
    -----------
    The ovs_snapshot_is_synced() shall check if the snapshot has been synced
    on the backend.

    Return Value
    ------------
    If the snapshot is synced on the backend the function shall return 1,
    if this is not the case (not synced) it shall return 0 otherwise -1 is
    returned and errno is set accordingly.

    Errors
    ------
    The function shall fail if:
        - ENOENT The named volume or snapshot doesn't exist
        - EINVAL Invalid arguments supplied
        - EIO An error occured during ovs_snapshot_remove()

List volumes
    int ovs_list_volumes(ovs_ctx_t *ctx, char *names, size_t *size);

    Description
    -----------
    The ovs_list_volumes() shall return a list of the volumes. The value
    'size' denotes the size of the 'names' buffer.
    If the expected size of the list is larger than 'size' then the
    aforementioned variable will be populated with the size of the volume
    list.

    Returne Value
    -------------
    Upon successful completion the function shall return the size of the
    list buffer otherwise -1 is returned.

    Errors
    ------
    The function shall fail if:
        - EINVAL Invalid arguments supplied
        - ERANGE The size of 'names' buffer is smaller than the expected size.
          The 'size' variable is populated with the expected size.
        - EIO An error occured during ovs_list_volumes()

Get volume status::
    int ovs_stat(ovs_ctx_t *ctx, struct stat *buf);

    Description
    -----------
    The ovs_stat() shall obtain information about an open volume
    associated with the volume context, and shall write it to the area pointed
    to by 'buf'.

    The 'buf' argument is a pointer to a stat structure, into which information
    is placed concerning the volume.

    Return Value
    ------------
    Upon successful completion, 0 shall be returned. Otherwise, -1 shall be
    returned and errno set to indicate the error.

    Errors
    ------
    The ovs_stat() function shall fail if:
        - EINVAL The context is not valid
        - EIO An I/O error occured while reading
        - EOVERFLOW The volume size in bytes or the number of blocks allocated
          to the volume cannot be represented correctly in the structure
          pointed to by 'buf'.

Read from a volume::
    ssize_t ovs_read(ovs_ctx_t *ctx,
                     void *buf,
                     size_t nbytes,
                     off_t offset);

    Description
    -----------
    The ovs_read() function shall attempt to read 'nbytes' bytes from the volume
    associated with the context, from a given position in the volume without
    changing the volume pointer, into the buffer pointed to by 'buf'. An attempt
    to perform ovs_read() on a volume that is incapable of seeking shall result
    in an error.

    Return Value
    ------------
    Upon successful completion, the function shall return a non-negative
    integer indicating the number of bytes actually read.
    Otherwise, the function shall return -1 and set errno to indicate the
    error.

    Errors
    ------
    The function shall fail if:
        - EBADF The context argument is not valid for reading.
        - EIO This error is implementation defined.
        - ENOBUFS Insufficient memory is available to fulfill the request.

Write to a volume::
    ssize_t ovs_write(ovs_ctx_t *ctx,
                      const void *buf,
                      size_t nbytes,
                      off_t offset);

    Description
    -----------
    The ovs_write() function shall attempt to write 'nbytes' bytes from the buffer
    pointed to by 'buf' to the volume associated with the context to a given
    position in the volume without changing the volume pointer. An attempt to
    perform an ovs_write() on a volume that is incapable of seeking shall
    result in an error.

    Return Value
    ------------
    Upon successful completion ovs_write() shall return the number of bytes
    written to the value associated with the context. This number shall never
    be greater than 'nbytes'. Otherwise, -1 shall be returned and errno set to
    indicate error.

    Errors
    ------
    The ovs_write() function shall fail if:
        - EBADF The context is not valid for writing.
        - EFBIG An attempt was made to write to a block that exceeds the
          implementation-defined maximum or the process size limit, and there
          was no room for any bytes to be written.
        - EFBIG The starting position is greater or equal at the offset
          maximum.
        - ENOSPC There was no free space remaining on the device containing
          the volume.

Synchronize changes to volume::
    int ovs_flush(ovs_ctx_t *ctx);

    Description
    -----------
    The ovs_flush() function shall request that all data for the volume is to
    be transferred to the storage device. The nature of the transfer is
    implementation-defined. The ovs_flush() function shall not return until the
    system has completed that action or until an error is detected.

    Return Value
    ------------
    Upon successful completion, ovs_flush() shall return 0.
    Otherwise -1 shall be returned and errno set to indicate the error. If the
    ovs_flush() function fails, outstanding I/O operations are not guaranteed
    to have been completed.

    Errors
    ------
    The ovs_flush() function shall fail if:
        - EBADF The context is not valid
        - EINVAL The context does not refer to a volume on which this
          operation is possible.
        - EIO An I/O error occured while reading from or writing to the
          volume.
    In the event that any of the queued I/O operation fail, ovs_flush() shall
    return the error conditions defined for ovs_read() and ovs_write();

Truncate an open volume to a specified length::
    int ovs_truncate(ovs_ctx_t *ctx,
                     uint64_t length);

    Description
    -----------
    The ovs_truncate() shall truncate an already open for writing volume to a
    size of precisely 'length' bytes.

    Return Value
    ------------
    Upon successful completion the function shall truncate the volume and shall
    return 0 if successful otherwise -1 is returned, no volume shall be
    truncated.

    Errors
    ------
    The function shall fail if:
        - EBADF Volume is not open for writing or is not open yet
        - EINVAL Invalid arguments supplied
        - EIO An error occured during ovs_truncate()

Asynchronous read from a volume::
    int ovs_aio_read(ovs_ctx_t *ctx,
                     struct ovs_aiocb *ovs_aiocbp);

    Description
    -----------
    The ovs_aio_read() function shall queue the I/O request
    described by the buffer pointed to by 'ovs_aiocbp'. This function is the
    asynchronous analog of ovs_read().

    The data is read starting at the absolute volue offset aio_offset,
    regardless of the current volume offset. After the call, the value of the
    current file offset is unspecified.

    This call returns as soon as the request has been enqueued; the read may
    or may not have completed when the call returns. One tests for completion
    using ovs_aio_return(). The return status of a completed I/O operation can
    be obtained by ovs_aio_return();

    Errors
    ------
    The ovs_aio_read() function shall fail if:
        - EAGAIN Out of resources
        - EBADF The context is not valid for reading
        - EINVAL One or more of aio_offset or aio_nbytes are invalid
        - ENOSYS ovs_aio_read() is not implemented
        - EOVERFLOW We start reading before the end of the volume and want at
          least one byte, but the starting position is past the maximum offset
          for this volume.

Asynchronous write to a volume::
    int ovs_aio_write(ovs_ctx_t *ctx,
                      struct ovs_aiocb *ovs_aiocbp);

    Description
    -----------
    The ovs_aio_write() function allows the calling process to
    write aio_nbytes from the buffer pointed to aio_buf to offset aio_offset.
    The function returns immediately after the write request has been
    enqueued; the write may or may not have completed at the time the call
    returns. If the request could not be enqueued, generally due to invalid
    arguments, the function returns without having enqueued the request.

    The ovs_aiocbp pointer may be subsequently used as an argument to
    ovs_aio_return() and ovs_aio_error() in order to determine return or error
    status for the enqueued operation while it is in progress;

    If the request is successfully enqueued, the value of aio_offset can be
    modified during the request, so this value must not be referenced after
    the request is enqueued.

    Return Value
    ------------
    Upon successful completion ovs_aio_write() function returns
    the value 0; otherwise the value -1 is returned and the variable errno is
    set to indicate the error.

    Errors
    ------
    The ovs_aio_write() function shall fail if:
        - EAGAIN Due to system resource limitations, the request was not
          queued.
        - ENOSYS The ovs_aio_write() function call is not supported.
        - EBADF The context is invalid, or is not opened for writing.
        - EINVAL The offset aio_offset is not valid, or the number of bytes
          specified by aio_nbytes is invalid.

    If the request is successfully enqueued, but subsequently canceled or an
    error occurs, the value returned by the ovs_aio_return() function call is
    per the ovs_write() function call, and the value returned by the
    ovs_aio_error() function call is either one of the error returns from the
    ovs_write() function, or one of:
        - EBADF The context is invalid for writing.
        - ECANCELED The request was explicitly cancelled via a call to
          ovs_aio_cancel()

Retrieve errors status for async I/O operation::
    int ovs_aio_error(ovs_ctx_t *ctx,
                      struct ovs_aiocb *ovs_aiocbp);

    Description
    -----------
    The ovs_aio_error() function returns the error status of the
    asynchronous I/O request associated with the structure pointed to by
    ovs_aiocbp.

    Return Value
    ------------
    If the asynchronous I/O request has completed successfully,
    ovs_aio_error() returns 0. If the request has not yet completed,
    EINPROGRESS is returned. If the request has completed unsuccessfully, the
    error status is returned as described in ovs_read(), ovs_write() or
    ovs_flush(). On failure, ovs_aio_error() returns -1 and sets errno to
    indicate the error condition.

    Errors
    ------
    The ovs_aio_error() function will fail if:
        - EINVAL The ovs_aiocbp argument does not reference an outstanding
          asynchronous I/O request.

Cancel an async I/O request::
    int ovs_aio_cancel(ovs_ctx_t *ctx,
                       struct ovs_aiocb *ovs_aiocbp);

    Description
    -----------
    The ovs_aio_cancel() function cancels the outstanding
    asynchronous I/O request for the volume specified by the context. If
    ovs_aiocbp is specified, only that specific asynchronous I/O request is
    cancelled.

    Normal asynchronous notification occurs for cancelled requests. Requests
    complete with an error result of ECANCELED.

    Return Value
    ------------
    The ovs_aio_cancel() function shall return -1 to indicate an
    error, or one of the following:
        - AIO_ALLDONE All the requests meeting the criteria have finished
        - AIO_CANCELED All outstanding requests meeting the criteria specified
          were cancelled.
        - AIO_NOTCANCELED Some requests were not cancelled, status for the
          requests should be checked with ovs_aio_error().
        - -1 An error occured. The cause of the error can be found by
          inspecting errno.

    Errors
    ------
    An error return from ovs_aio_cancel() indicates:
        - EBADF The context or the request is not valid
        - ENOSYS The ovs_aio_cancel() function call is not supported.

Retrieve return status of an async I/O operation::
    ssize_t ovs_aio_return(ovs_ctx_t *ctx, struct ovs_aiocb *ovs_aiocbp);

    Description
    -----------
    The ovs_aio_return() function returns the final status for
    the asynchronous I/O request with control pointed to by ovs_aiocbp. This
    function should be called only once for any given request after
    ovs_aio_error() return something other than EINPROGRESS.

    Return Value
    ------------
    If the asynchronous I/O operation has completed, this
    function returns the value that would have been returned in case of a
    synchronous ovs_read(), ovs_write() or ovs_flush() function call.

    If the asynchronous I/O operation has not yet completed, the return value
    and effect of ovs_aio_return() are undefined.

    Errors
    ------
    The ovs_aio_return() function will fail if:
        - EINVAL ovs_aiocbp does not point at a control block of asynchronous
          I/O request of which the return status has not been retrieved yet.
        - ENOSYS ovs_aio_return() is not implemented.

Finish an async I/O operation:
    int ovs_aio_finish(ovs_ctx_t *ctx, struct ovs_aiocb* ovs_aiocbp);

    Description
    -----------
    The ovs_aio_finish() function finishes the asynchronous I/O request with
    control block pointed to by ovs_aiocbp. This function should be called
    only once for any given request after ovs_aio_return().

    Return Value
    ------------
    The ovs_aio_finish() function shall return -1 on error and 0 on success.

    Errors
    ------
    The ovs_aio_finish() will fail if:
        - EINVAL The context or the request is not valid

Suspend until asynchronous I/O operation or timeout complete::
int ovs_aio_suspend(ovs_ctx_t *ctx,
                    struct ovs_aiocb *ovs_aiocbp,
                    const struct timespec *timeout);

    Description
    -----------
    The ovs_aio_suspend() suspends the calling process until the
    specified asynchronous I/O request has completed, or the timeout has
    passed.

    If timeout is a non-nil pointer, it specifies a maximum interval suspend.
    If timeout is a nil pointer, the suspend blocks indefinitely.

    Return Values
    -------------
    If the specified I/O request has completed, ovs_aio_suspend() returns 0.
    Otherwise, it returns -1 and sets errno to indicate the error, as enumerated below.

    Errors
    ------
    An error return from ovs_aio_suspend indicates:
        - EAGAIN The timeout expired before the I/O request has completed.
        - EINTR The suspend was interrupted by a signal.
        - EINVAL The I/O request or the context is not valid.

Construct a completion::
    ovs_completion_t *ovs_aio_create_completion(ovs_callback_t complete_cb,
                                                void *arg);

    Description
    -----------
    The ovs_aio_create_completion() function constructs a
    completion to use with asynchronous operations.

    The complete_cb() callbacks are enqueued in order of receipt in the
    completion queue and executed asynchronously.

    Return Value
    ------------
    On success this function returns an ovs_completion_t structure. On error
    NULL is returned, and errno is set appropriately.

    Errors
    ------
    An error return from ovs_aio_create_completion indicates:
        - EINVAL Invalid arguments. The most usual error is if complete_cb is
          NULL.

Retrieve return status of a completion::
    int ovs_aio_return_completion(ovs_completion_t *comp);

    Description
    -----------
    The ovs_aio_return_completion() function returns the return value of
    a completion.

    Return Value
    ------------
    Returns the return value of the called completion function.

    If the completion has not yet finished -1 is returned.

    Note: The function should be called either inside the completion callback
    or after ovs_aio_wait_for_completion() has returned.

Wait for completion to finish::
    int ovs_aio_wait_completion(ovs_completion_t *comp,
                                const struct timespec *timeout);

    Description
    -----------
    The ovs_aio_wait_completion() suspends the calling process until the
    specified completion has completed, or the timeout has passed.

    If timeout is a non-nil pointer, it specifies a maximum interval suspend.
    If timeout is a nil pointer, the suspend blocks indefinitely.

    Return Value
    ------------
    If the specified completion has completed, ovs_wait_completion() returns 0.
    Otherwise, it returns -1 and sets errno to indicate the error, as
    enumereated below.

    Errors
    ------
    An error return from ovs_aio_wait_completion indicates:
        - EINTR The timeout expired before completion has completed.
        - EINVAL The completion is not valid.

Unblock a thread waiting for a completion::
    int ovs_aio_signal_completion(ovs_completion_t *comp);

    Description
    -----------
    The ovs_aio_signal_completion() unblocks a thread waiting for the
    completion 'comp' to complete.

    Return Values
    -------------
    If successful the function will return zero. Otherwise, an error number
    will be returned to indicate the error.

    Errors
    ------
    An error return from ovs_aio_signal_completion indicates:
        - EINVAL The value specified by 'comp' is invalid.

Release a completion::
    int ovs_aio_release_completion(ovs_completion_t *comp);

    Description
    -----------
    The ovs_aio_release_completion() function releases a completion.

    It may not be freed immediately if the operations has not yet been
    commited or has been executed.

Asynchronous read from a volume::
    int ovs_aio_readcb(ovs_ctx_t *ctx,
                       struct ovs_aiocb *ovs_aiocbp,
                       ovs_completion_t *comp);

    Description
    -----------
    The same as ovs_aio_read() but on finish execute completion
    'comp'.

Asynchronous write to a volume::
    int ovs_aio_writecb(ovs_ctx_t *ctx,
                        struct ovs_aiocb *ovs_aiocbp,
                        ovs_completion_t *comp);

    Description
    -----------
    The same as ovs_aio_write() but on finish execute completion
    'comp'.

Asynchronous volume synchronization::
    int ovs_aio_flushcb(ovs_ctx_t *ctx, ovs_completion_t *comp);

    Description
    -----------
    The ovs_aio_flushcb() function does a sync on all outstanding
    asynchronous I/O operations associated with the context and on finish
    executes completion 'comp'.

    Return Value
    ------------
    On success this function returns 0. On error -1 is returned,
    and errno is set appropriately.

    Errors
    ------
    An error return from ovs_aio_flush() indicates:
        - EAGAIN Out of resources
        - EBADF Context is not valid for writing
        - EINVAL Synchronized I/O is not supported for this volume
        - ENOSYS ovs_aio_flushcb() is not implemented

Shared memory allocation::
    ovs_buffer_t* ovs_allocate(ovs_ctx_t *ctx,
                               size_t size)

    Description
    -----------
    Allocate shared memory of the specified 'size' to be sent in zero-copy
    fashion. The content of the buffer is undefined after allocation and it
    should be filled in by the user.

    Return Value
    ------------
    If the function succeeds pointer to newly allocated buffer is returned.
    Otherwise, NULL is returned and errno is set to one of the values defined
    below.

    Errors
    ------
        - ENOMEM Not enough memory to allocate the specified size

Retrieve pointer to buffer content::
    void *ovs_buffer_data(ovs_buffer_t *ptr)

    Description
    -----------
    Return pointer to the buffer content previously allocated by
    ovs_allocate().

    Return Value
    ------------
    If the function succeeds pointer to the allocated buffer is returned.
    Otherwise NULL is returned and errno is set to one of the values defined
    below.

    Errors
    ------
        - EINVAL The value specified by 'ptr' is invalid.

Retrieve size of buffer::
    size_t ovs_buffer_size(ovs_buffer_t *ptr)

    Description
    -----------
    Return size of the buffer content previously allocated by
    ovs_allocate().

    Return Value
    ------------
    If the function succeeds the size of the buffer content is returned.
    Otherwise -1 is returned and errno is set to one of the values defined
    below.

    Errors
    ------
        - EINVAL The value specified by 'ptr' is invalid.

Shared memory deallocation::
    int ovs_deallocate(ovs_ctx_t *ctx,
                       ovs_buffer_t *shptr)

    Description
    -----------
    Deallocates a buffer allocated using ovs_allocate() function.

    Return Value
    ------------
    If the function succeeds zero is returned. Otherwise, -1 is returned and
    errno is set to one of the values defined below.

    Errors
    ------
        - EFAULT The buffer pointer is invalid.

AUTHORS
-------
Chrysostomos Nanakos <cnanakos@openvstorage.com>
