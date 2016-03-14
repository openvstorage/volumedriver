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

#include "volumedriver.h"
#include "common.h"
#include "ShmHandler.h"
#include "tracing.h"
#include "../ShmControlChannelProtocol.h"
#include "../ShmClient.h"

#include <cerrno>
#include <limits.h>
#include <map>

#include <youtils/SpinLock.h>
#include <youtils/System.h>
#include <youtils/IOException.h>
#include <youtils/ScopeExit.h>

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

struct ovs_context_t
{
    TransportType transport;
    std::string host;
    int port;
    ovs_shm_context *shm_ctx_;
};

static bool
_is_volume_name_valid(const char *volume_name)
{
    if (volume_name == NULL || strlen(volume_name) == 0 ||
        strlen(volume_name) >= NAME_MAX)
    {
        return false;
    }
    else
    {
        return true;
    }
}

ovs_ctx_attr_t*
ovs_ctx_attr_new()
{
    try
    {
        ovs_ctx_attr_t *attr = new ovs_ctx_attr_t;
        attr->transport = TransportType::SharedMemory;
        attr->host = NULL;
        attr->port = 0;
        return attr;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    return NULL;
}

int
ovs_ctx_attr_destroy(ovs_ctx_attr_t *attr)
{
    if (attr)
    {
        if (attr->host)
        {
            free(attr->host);
        }
        free(attr);
    }
    return 0;
}

int
ovs_ctx_attr_set_transport(ovs_ctx_attr_t *attr,
                           const char *transport,
                           const char *host,
                           int port)
{
    if (attr == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (transport == NULL or (transport and (not strcmp(transport, "shm"))))
    {
        attr->transport = TransportType::SharedMemory;
        return 0;
    }

    if ((not strcmp(transport, "tcp")) and host)
    {
        attr->transport = TransportType::TCP;
        attr->host = strdup(host);
        attr->port = port;
        return 0;
    }

    if ((not strcmp(transport, "rdma")) and host)
    {
        attr->transport = TransportType::RDMA;
        attr->host = strdup(host);
        attr->port = port;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

ovs_ctx_t*
ovs_ctx_new(const ovs_ctx_attr_t *attr)
{
    ovs_ctx_t *ctx = NULL;

    if(not attr)
    {
        errno = EINVAL;
        return NULL;
    }
    try
    {
        ctx = new ovs_ctx_t;
        ctx->transport = attr->transport;
        ctx->host = std::string(attr->host ? attr->host : "");
        ctx->port = attr->port;
        ctx->shm_ctx_ = nullptr;
        return ctx;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    return NULL;
}

int
ovs_ctx_init(ovs_ctx_t *ctx,
             const char* volume_name,
             int oflag)
{
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_ctx_init_enter,
               volume_name,
               oflag);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_ctx_init_exit,
                                          ctx,
                                          errno);
                }));

    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL;
        return -1;
    }
    if (oflag != O_RDONLY &&
        oflag != O_WRONLY &&
        oflag != O_RDWR)
    {
        errno = EINVAL;
        return -1;
    }

    /* On Error: EACCESS or EIO */
    try
    {
        ctx->shm_ctx_ = new ovs_shm_context(volume_name,
                                                          oflag);
        return 0;
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = EACCES;
    }
    catch (const ShmIdlInterface::VolumeNameAlreadyRegistered&)
    {
        errno = EBUSY;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
    }
    catch (...)
    {
        errno = EIO;
    }
    return -1;
}

int
ovs_ctx_destroy(ovs_ctx_t *ctx)
{
    int r = 0;

    auto on_exit(youtils::make_scope_exit([&]
                {
                    tracepoint(openvstorage_libovsvolumedriver,
                               ovs_ctx_destroy,
                               ctx,
                               r);
                }));

    if (ctx == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }
    if (ctx->shm_ctx_)
    {
        delete ctx->shm_ctx_;
    }
    delete ctx;
    return r;
}

int
ovs_create_volume(ovs_ctx_t *ctx ATTRIBUTE_UNUSED,
                  const char* volume_name,
                  uint64_t size)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_create_volume_enter,
               volume_name,
               size);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_create_volume_exit,
                                          volume_name,
                                          r,
                                          errno);
                }));

    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL; r = -1;
        return r;
    }

    try
    {
        volumedriverfs::ShmClient::create_volume(volume_name, size);
    }
    catch (const ShmIdlInterface::VolumeExists&)
    {
        errno = EEXIST; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
ovs_remove_volume(ovs_ctx_t *ctx ATTRIBUTE_UNUSED,
                  const char* volume_name)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_remove_volume_enter,
               volume_name);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_remove_volume_exit,
                                          volume_name,
                                          r,
                                          errno);
                }));

    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL; r = -1;
        return r;
    }

    try
    {
        volumedriverfs::ShmClient::remove_volume(volume_name);
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
ovs_snapshot_create(ovs_ctx_t *ctx ATTRIBUTE_UNUSED,
                    const char* volume_name,
                    const char* snapshot_name,
                    const int64_t timeout)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_snapshot_create_enter,
               volume_name,
               snapshot_name,
               timeout);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_snapshot_create_exit,
                                          volume_name,
                                          snapshot_name,
                                          r,
                                          errno);
                }));

    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL; r = -1;
        return r;
    }

    try
    {
        volumedriverfs::ShmClient::create_snapshot(volume_name,
                                                   snapshot_name,
                                                   timeout);
    }
    catch (const ShmIdlInterface::PreviousSnapshotNotOnBackendException&)
    {
        errno = EBUSY; r = -1;
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT; r = -1;
    }
    catch (const ShmIdlInterface::SyncTimeoutException&)
    {
        errno = ETIMEDOUT; r = -1;
    }
    catch (const ShmIdlInterface::SnapshotAlreadyExists&)
    {
        errno = EEXIST; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
ovs_snapshot_rollback(ovs_ctx_t *ctx ATTRIBUTE_UNUSED,
                      const char* volume_name,
                      const char* snapshot_name)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_snapshot_rollback_enter,
               volume_name,
               snapshot_name);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_snapshot_rollback_exit,
                                          volume_name,
                                          snapshot_name,
                                          r,
                                          errno);
                }));

    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL; r= -1;
        return r;
    }

    try
    {
        volumedriverfs::ShmClient::rollback_snapshot(volume_name,
                                                     snapshot_name);
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT; r = -1;
    }
    catch (const ShmIdlInterface::VolumeHasChildren&)
    {
        errno = ENOTEMPTY; r = -1;
        //errno = ECHILD; r= -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
ovs_snapshot_remove(ovs_ctx_t *ctx ATTRIBUTE_UNUSED,
                    const char* volume_name,
                    const char* snapshot_name)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_snapshot_remove_enter,
               volume_name,
               snapshot_name);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_snapshot_remove_exit,
                                          volume_name,
                                          snapshot_name,
                                          r,
                                          errno);
                }));

    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL;
        return (r = -1);
    }

    try
    {
        volumedriverfs::ShmClient::delete_snapshot(volume_name,
                                                   snapshot_name);
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT; r = -1;
    }
    catch (const ShmIdlInterface::VolumeHasChildren&)
    {
        errno = ENOTEMPTY; r = -1;
        //errno = ECHILD; r = -1;
    }
    catch (const ShmIdlInterface::SnapshotNotFound&)
    {
        errno = ENOENT; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
ovs_snapshot_list(ovs_ctx_t *ctx ATTRIBUTE_UNUSED,
                  const char* volume_name,
                  ovs_snapshot_info_t *snap_list,
                  int *max_snaps)
{
    int i, len;
    uint64_t size = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_snapshot_list_enter,
               volume_name,
               snap_list);

    if (!max_snaps)
    {
        tracepoint(openvstorage_libovsvolumedriver,
                   ovs_snapshot_list_exit,
                   volume_name,
                   snap_list,
                   -1,
                   0,
                   EINVAL);
        errno = EINVAL;
        return -1;
    }

    if (not _is_volume_name_valid(volume_name))
    {
        tracepoint(openvstorage_libovsvolumedriver,
                   ovs_snapshot_list_exit,
                   volume_name,
                   snap_list,
                   -1,
                   *max_snaps,
                   EINVAL);
        errno = EINVAL;
        return -1;
    }

    int saved_errno = 0;
    std::vector<std::string> snaps;
    try
    {
        snaps = volumedriverfs::ShmClient::list_snapshots(volume_name,
                                                          &size);
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        saved_errno = ENOENT;
    }
    catch (...)
    {
        saved_errno = EIO;
    }

    if (saved_errno)
    {
        tracepoint(openvstorage_libovsvolumedriver,
                   ovs_snapshot_list_exit,
                   volume_name,
                   snap_list,
                   -1,
                   *max_snaps,
                   saved_errno);
        errno = saved_errno;
        return -1;
    }

    len = snaps.size();
    if (*max_snaps < len + 1)
    {
        *max_snaps = len + 1;
        tracepoint(openvstorage_libovsvolumedriver,
                   ovs_snapshot_list_exit,
                   volume_name,
                   snap_list,
                   -1,
                   *max_snaps,
                   ERANGE);
        errno = ERANGE;
        return -1;
    }

    for (i = 0; i < len; i++)
    {
        snap_list[i].name = strdup(snaps[i].c_str());
        snap_list[i].size = size;
        if (!snap_list[i].name)
        {
            for (int j = 0; j < i; j++)
            {
                free(const_cast<char*>(snap_list[j].name));
            }
            tracepoint(openvstorage_libovsvolumedriver,
                       ovs_snapshot_list_exit,
                       volume_name,
                       snap_list,
                       -1,
                       *max_snaps,
                       ENOMEM);
            errno = ENOMEM;
            return -1;
        }
    }

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_snapshot_list_exit,
               volume_name,
               snap_list,
               len,
               *max_snaps,
               0);
    snap_list[i].name = NULL;
    snap_list[i].size = 0;
    return len;
}

void
ovs_snapshot_list_free(ovs_snapshot_info_t *snap_list)
{
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_snapshot_list_free_enter,
               snap_list);

    if (snap_list)
    {
        while (snap_list->name)
        {
            free(const_cast<char*>(snap_list->name));
            snap_list++;
        }
    }
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_snapshot_list_free_exit);
}

int
ovs_snapshot_is_synced(ovs_ctx_t *ctx ATTRIBUTE_UNUSED,
                       const char* volume_name,
                       const char* snapshot_name)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_snapshot_is_synced_enter,
               volume_name,
               snapshot_name);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_snapshot_is_synced_exit,
                                          volume_name,
                                          snapshot_name,
                                          r,
                                          errno);
                }));

    if (not _is_volume_name_valid(volume_name))
    {
        errno = EINVAL; r = -1;
        return r;
    }

    try
    {
        r = volumedriverfs::ShmClient::is_snapshot_synced(volume_name,
                                                          snapshot_name);
    }
    catch (const ShmIdlInterface::VolumeDoesNotExist&)
    {
        errno = ENOENT; r = -1;
    }
    catch (const ShmIdlInterface::SnapshotNotFound&)
    {
        errno = ENOENT; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

int
ovs_list_volumes(ovs_ctx_t *ctx ATTRIBUTE_UNUSED,
                 char *names,
                 size_t *size)
{
    std::vector<std::string> volumes;
    size_t expected_size = 0;
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_list_volumes_enter,
               names,
               *size);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_list_volumes_exit,
                                          names,
                                          *size,
                                          r,
                                          errno);
                }));
    try
    {
        volumes = volumedriverfs::ShmClient::list_volumes();
    }
    catch (...)
    {
        errno = EIO;
        return (r = -1);
    }

    for (size_t t = 0; t < volumes.size(); t++)
    {
        expected_size += volumes[t].size() + 1;
    }

    if (*size < expected_size)
    {
        *size = expected_size;
        errno = ERANGE;
        return (r = -1);
    }

    if (!names)
    {
        errno = EINVAL;
        return (r = -1);
    }

    for (int i = 0; i < (int)volumes.size(); i++)
    {
        const char* name = volumes[i].c_str();
        strcpy(names, name);
        names += strlen(name) + 1;
    }
    r = (int)expected_size;
    return r;
}

static int
_ovs_submit_aio_request(ovs_ctx_t *ctx,
                        struct ovs_aiocb *ovs_aiocbp,
                        ovs_completion_t *completion,
                        const RequestOp& op)
{
    int r, accmode;
    ovs_shm_context *shm_ctx_ = ctx->shm_ctx_;

    ovs_submit_aio_request_tracepoint_enter(op,
                                            ctx,
                                            ovs_aiocbp,
                                            completion);

    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        ovs_submit_aio_request_tracepoint_exit(op,
                                               ctx,
                                               ovs_aiocbp,
                                               completion,
                                               -1,
                                               EINVAL);
        errno = EINVAL;
        return -1;
    }

    if ((ovs_aiocbp->aio_nbytes <= 0 ||
         ovs_aiocbp->aio_offset < 0) &&
         op != RequestOp::Flush && op != RequestOp::AsyncFlush)
    {
        ovs_submit_aio_request_tracepoint_exit(op,
                                               ctx,
                                               ovs_aiocbp,
                                               completion,
                                               -1,
                                               EINVAL);
        errno = EINVAL;
        return -1;
    }

    accmode = shm_ctx_->oflag & O_ACCMODE;
    switch (op)
    {
    case RequestOp::Read:
        if (accmode == O_WRONLY)
        {
            ovs_submit_aio_request_tracepoint_exit(op,
                                                   ctx,
                                                   ovs_aiocbp,
                                                   completion,
                                                   -1,
                                                   EBADF);
            errno = EBADF;
            return -1;
        }
        break;
    case RequestOp::Write:
    case RequestOp::Flush:
    case RequestOp::AsyncFlush:
        if (accmode == O_RDONLY)
        {
            ovs_submit_aio_request_tracepoint_exit(op,
                                                   ctx,
                                                   ovs_aiocbp,
                                                   completion,
                                                   -1,
                                                   EBADF);
            errno = EBADF;
            return -1;
        }
        break;
    default:
        errno = EBADF;
        return -1;
    }

    ovs_aio_request *request;
    try
    {
        request = new ovs_aio_request;
        request->ovs_aiocbp = ovs_aiocbp;
        request->completion = completion;
        request->_op = op;
    }
    catch (std::bad_alloc&)
    {
        ovs_submit_aio_request_tracepoint_exit(op,
                                               ctx,
                                               ovs_aiocbp,
                                               completion,
                                               -1,
                                               ENOMEM);
        errno = ENOMEM;
        return -1;
    }

    pthread_cond_init(&request->_cond, NULL);
    pthread_mutex_init(&request->_mutex, NULL);
    request->_on_suspend = false;
    request->_canceled = false;
    request->_completed = false;
    request->_signaled = false;
    request->_rv = 0;
    ovs_aiocbp->request_ = request;
    switch (op)
    {
    case RequestOp::Read:
        /* on error returns -1 */
        r = shm_ctx_->shm_client_->send_read_request(ovs_aiocbp->aio_buf,
                                                     ovs_aiocbp->aio_nbytes,
                                                     ovs_aiocbp->aio_offset,
                                                     reinterpret_cast<void*>(request));
        break;
    case RequestOp::Write:
    case RequestOp::Flush:
    case RequestOp::AsyncFlush:
        /* on error returns -1 */
        r = shm_ctx_->shm_client_->send_write_request(ovs_aiocbp->aio_buf,
                                                      ovs_aiocbp->aio_nbytes,
                                                      ovs_aiocbp->aio_offset,
                                                      reinterpret_cast<void*>(request));
        break;
    default:
        r = -1;
    }
    if (r < 0)
    {
        delete request;
        errno = EIO;
    }
    int saved_errno = errno;
    ovs_submit_aio_request_tracepoint_exit(op,
                                           ctx,
                                           ovs_aiocbp,
                                           completion,
                                           r,
                                           saved_errno);
    errno = saved_errno;
    return r;
}

int
ovs_aio_read(ovs_ctx_t *ctx,
             struct ovs_aiocb *ovs_aiocbp)
{
    return _ovs_submit_aio_request(ctx,
                                   ovs_aiocbp,
                                   NULL,
                                   RequestOp::Read);
}

int
ovs_aio_write(ovs_ctx_t *ctx,
              struct ovs_aiocb *ovs_aiocbp)
{
    return _ovs_submit_aio_request(ctx,
                                   ovs_aiocbp,
                                   NULL,
                                   RequestOp::Write);
}

int
ovs_aio_error(ovs_ctx_t *ctx,
              const struct ovs_aiocb *ovs_aiocbp)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_aio_error_enter,
               ctx,
               ovs_aiocbp);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_aio_error_exit,
                                          ctx,
                                          ovs_aiocbp,
                                          r,
                                          errno);
                }));

    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }

    if (ovs_aiocbp->request_->_canceled)
    {
        return (r = ECANCELED);
    }

    if (not ovs_aiocbp->request_->_completed)
    {
        return (r = EINPROGRESS);
    }

    if (ovs_aiocbp->request_->_failed)
    {
        return (r = ovs_aiocbp->request_->_errno);
    }
    else
    {
        return r;
    }
}

ssize_t
ovs_aio_return(ovs_ctx_t *ctx,
               struct ovs_aiocb *ovs_aiocbp)
{
    int r;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_aio_return_enter,
               ctx,
               ovs_aiocbp);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_aio_return_exit,
                                          ctx,
                                          ovs_aiocbp,
                                          r,
                                          errno);
                }));

    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }

    errno = ovs_aiocbp->request_->_errno;
    if (not ovs_aiocbp->request_->_failed)
    {
        r = ovs_aiocbp->request_->_rv;
    }
    else
    {
        //errno = ovs_aiocbp->request_->_errno;
        r = -1;
    }
    return r;
}

int
ovs_aio_finish(ovs_ctx_t *ctx,
               struct ovs_aiocb *ovs_aiocbp)
{
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_aio_finish,
               ctx,
               ovs_aiocbp);

    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    pthread_cond_destroy(&ovs_aiocbp->request_->_cond);
    pthread_mutex_destroy(&ovs_aiocbp->request_->_mutex);
    delete ovs_aiocbp->request_;
    return 0;
}

int
ovs_aio_suspend(ovs_ctx_t *ctx,
                ovs_aiocb *ovs_aiocbp,
                const struct timespec *timeout)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_aio_suspend_enter,
               ctx,
               ovs_aiocbp,
               timeout);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_aio_suspend_exit,
                                          ctx,
                                          ovs_aiocbp,
                                          timeout,
                                          r,
                                          errno);
                }));

    if (ctx == NULL || ovs_aiocbp == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }
    if (__sync_bool_compare_and_swap(&ovs_aiocbp->request_->_on_suspend,
                                     false,
                                     true,
                                     __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&ovs_aiocbp->request_->_mutex);
        while (not ovs_aiocbp->request_->_signaled)
        {
            if (timeout)
            {
                r = pthread_cond_timedwait(&ovs_aiocbp->request_->_cond,
                                           &ovs_aiocbp->request_->_mutex,
                                           timeout);
            }
            else
            {
                r = pthread_cond_wait(&ovs_aiocbp->request_->_cond,
                                      &ovs_aiocbp->request_->_mutex);
            }
        }
        pthread_mutex_unlock(&ovs_aiocbp->request_->_mutex);
    }
    return r;
}

int
ovs_aio_cancel(ovs_ctx_t * /*ctx*/,
               struct ovs_aiocb * /*ovs_aiocbp*/)
{
    errno = ENOSYS;
    return -1;
}

ovs_buffer_t*
ovs_allocate(ovs_ctx_t *ctx,
             size_t size)
{
    ovs_buffer_t *buf = ctx->shm_ctx_->cache_->allocate(size);
    if (!buf)
    {
        errno = ENOMEM;
    }
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_allocate,
               ctx,
               buf,
               size);
    return buf;
}

void*
ovs_buffer_data(ovs_buffer_t *ptr)
{
   tracepoint(openvstorage_libovsvolumedriver,
              ovs_buffer_data,
              ptr);

    if (likely(ptr != NULL))
    {
        return ptr->buf;
    }
    else
    {
        errno = EINVAL;
        return NULL;
    }
}

size_t
ovs_buffer_size(ovs_buffer_t *ptr)
{
   tracepoint(openvstorage_libovsvolumedriver,
              ovs_buffer_size,
              ptr);

    if (likely(ptr != NULL))
    {
        return ptr->size;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

int
ovs_deallocate(ovs_ctx_t *ctx,
               ovs_buffer_t *ptr)
{
    int r = ctx->shm_ctx_->cache_->deallocate(ptr);
    if (r < 0)
    {
        errno = EFAULT;
    }
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_deallocate,
               ctx,
               ptr,
               r);
    return r;
}

ovs_completion_t*
ovs_aio_create_completion(ovs_callback_t complete_cb,
                          void *arg)
{
    ovs_completion_t *completion = NULL;

    auto on_exit(youtils::make_scope_exit([&]
                {
                    tracepoint(openvstorage_libovsvolumedriver,
                               ovs_aio_create_completion,
                               complete_cb,
                               arg,
                               completion);
                }));

    if (complete_cb == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    try
    {
        completion = new ovs_completion_t;
        completion->complete_cb = complete_cb;
        completion->cb_arg = arg;
        completion->_calling = false;
        completion->_on_wait = false;
        completion->_signaled = false;
        completion->_failed = false;
        pthread_cond_init(&completion->_cond, NULL);
        pthread_mutex_init(&completion->_mutex, NULL);
        return completion;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return NULL;
    }
}

ssize_t
ovs_aio_return_completion(ovs_completion_t *completion)
{
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_aio_return_completion,
               completion);

    if (completion == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (not completion->_calling)
    {
        return -1;
    }
    else
    {
        if (not completion->_failed)
        {
            return completion->_rv;
        }
        else
        {
            errno = EIO;
            return -1;
        }
    }
}

int
ovs_aio_wait_completion(ovs_completion_t *completion,
                        const struct timespec *timeout)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_aio_wait_completion_enter,
               completion,
               timeout);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_aio_wait_completion_exit,
                                          completion,
                                          timeout,
                                          r,
                                          errno);
                }));

    if (completion == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }

    if (__sync_bool_compare_and_swap(&completion->_on_wait,
                                     false,
                                     true,
                                     __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&completion->_mutex);
        while (not completion->_signaled)
        {
            if (timeout)
            {
                r = pthread_cond_timedwait(&completion->_cond,
                                           &completion->_mutex,
                                           timeout);
            }
            else
            {
                r = pthread_cond_wait(&completion->_cond,
                                      &completion->_mutex);
            }
        }
        pthread_mutex_unlock(&completion->_mutex);
    }
    return r;
}

int
ovs_aio_signal_completion(ovs_completion_t *completion)
{
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_aio_signal_completion,
               completion);

    if (completion == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    if (not __sync_bool_compare_and_swap(&completion->_on_wait,
                                         false,
                                         true,
                                         __ATOMIC_RELAXED))
    {
        pthread_mutex_lock(&completion->_mutex);
        completion->_signaled = true;
        pthread_cond_signal(&completion->_cond);
        pthread_mutex_unlock(&completion->_mutex);
    }
    return 0;
}

int
ovs_aio_release_completion(ovs_completion_t *completion)
{
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_aio_release_completion,
               completion);

    if (completion == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_destroy(&completion->_mutex);
    pthread_cond_destroy(&completion->_cond);
    delete completion;
    return 0;
}

int
ovs_aio_readcb(ovs_ctx_t *ctx,
               struct ovs_aiocb *ovs_aiocbp,
               ovs_completion_t *completion)
{
    return _ovs_submit_aio_request(ctx,
                                   ovs_aiocbp,
                                   completion,
                                   RequestOp::Read);
}

int
ovs_aio_writecb(ovs_ctx_t *ctx,
                struct ovs_aiocb *ovs_aiocbp,
                ovs_completion_t *completion)
{
    return _ovs_submit_aio_request(ctx,
                                   ovs_aiocbp,
                                   completion,
                                   RequestOp::Write);
}

int
ovs_aio_flushcb(ovs_ctx_t *ctx,
                ovs_completion_t *completion)
{
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_aio_flushcb_enter,
               ctx);

    if (ctx == NULL)
    {
        tracepoint(openvstorage_libovsvolumedriver,
                   ovs_aio_flushcb_exit,
                   ctx,
                   -1,
                   EINVAL);
        errno = EINVAL;
        return -1;
    }

    ovs_aiocb *aio = new ovs_aiocb;
    if (aio == NULL)
    {
        tracepoint(openvstorage_libovsvolumedriver,
                   ovs_aio_flushcb_exit,
                   ctx,
                   -1,
                   ENOMEM);
        errno = ENOMEM;
        return -1;
    }
    aio->aio_nbytes = 0;
    aio->aio_offset = 0;
    aio->aio_buf = NULL;

    return _ovs_submit_aio_request(ctx,
                                   aio,
                                   completion,
                                   RequestOp::AsyncFlush);
}

ssize_t
ovs_read(ovs_ctx_t *ctx,
         void *buf,
         size_t nbytes,
         off_t offset)
{
    ssize_t r;
    struct ovs_aiocb aio;
    aio.aio_buf = buf;
    aio.aio_nbytes = nbytes;
    aio.aio_offset = offset;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_read_enter,
               ctx,
               buf,
               nbytes,
               offset);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_read_exit,
                                          ctx,
                                          buf,
                                          r,
                                          errno);
                }));

    if (ctx == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }

    if ((r = ovs_aio_read(ctx, &aio)) < 0)
    {
        return r;
    }

    if ((r = ovs_aio_suspend(ctx, &aio, NULL)) < 0)
    {
        return r;
    }

    r = ovs_aio_return(ctx, &aio);
    if (ovs_aio_finish(ctx, &aio) < 0)
    {
        r = -1;
    }
    return r;
}

ssize_t
ovs_write(ovs_ctx_t *ctx,
          const void *buf,
          size_t nbytes,
          off_t offset)
{
    ssize_t r;
    struct ovs_aiocb aio;
    aio.aio_buf = const_cast<void*>(buf);
    aio.aio_nbytes = nbytes;
    aio.aio_offset = offset;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_write_enter,
               ctx,
               buf,
               nbytes,
               offset);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_write_exit,
                                          ctx,
                                          buf,
                                          r,
                                          errno);
                }));

    if (ctx == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }

    if ((r = ovs_aio_write(ctx, &aio)) < 0)
    {
        return r;
    }

    if ((r = ovs_aio_suspend(ctx, &aio, NULL)) < 0)
    {
        return r;
    }

    r = ovs_aio_return(ctx, &aio);
    if (ovs_aio_finish(ctx, &aio) < 0)
    {
        r = -1;
    }
    return r;
}

int
ovs_stat(ovs_ctx_t *ctx, struct stat *st)
{
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_stat_enter,
               ctx,
               st);

    if (ctx == NULL || st == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    int r = ctx->shm_ctx_->shm_client_->stat(st);
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_stat_exit,
               ctx,
               st,
               r,
               st->st_blksize,
               st->st_size);
    return r;
}

int
ovs_flush(ovs_ctx_t *ctx)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_flush_enter,
               ctx);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_flush_exit,
                                          ctx,
                                          r,
                                          errno);
                }));

    if (ctx == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }

    struct ovs_aiocb aio;
    aio.aio_nbytes = 0;
    aio.aio_offset = 0;
    aio.aio_buf = NULL;

    if ((r = _ovs_submit_aio_request(ctx,
                                     &aio,
                                     NULL,
                                     RequestOp::Flush)) < 0)
    {
        return r;
    }

    if ((r = ovs_aio_suspend(ctx, &aio, NULL)) < 0)
    {
        return r;
    }

    r = ovs_aio_return(ctx, &aio);
    if (ovs_aio_finish(ctx, &aio) < 0)
    {
        r = -1;
    }
    return r;
}
