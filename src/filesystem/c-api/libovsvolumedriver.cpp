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
#include "tracing.h"
#include "context.h"
#include "ShmContext.h"
#include "NetworkHAContext.h"
#include "Utils.h"
#include "Logger.h"

#include <youtils/SpinLock.h>
#include <youtils/System.h>
#include <youtils/IOException.h>
#include <youtils/ScopeExit.h>

#include <limits.h>
#include <libxio.h>

#include <vector>
#include <cerrno>
#include <map>

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

namespace libvoldrv = libovsvolumedriver;

namespace
{

libvoldrv::DefaultRequestDispatcherCallback default_request_callback;

}

ovs_ctx_attr_t*
ovs_ctx_attr_new()
{
    try
    {
        ovs_ctx_attr_t *attr = new ovs_ctx_attr_t;
        attr->transport = TransportType::Error;
        attr->port = 0;
        attr->network_qdepth = 256;
        attr->enable_ha = false;
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
        delete attr;
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
        attr->port = port;
        return hostname_to_ip(host, attr->host);
    }

    if ((not strcmp(transport, "rdma")) and host)
    {
        attr->transport = TransportType::RDMA;
        attr->port = port;
        return hostname_to_ip(host, attr->host);
    }
    errno = EINVAL;
    return -1;
}

int
ovs_ctx_attr_set_network_qdepth(ovs_ctx_attr_t *attr,
                                const uint64_t qdepth)
{
    if (attr == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    if (qdepth <= 0)
    {
        errno = EINVAL;
        return -1;
    }
    attr->network_qdepth = qdepth;
    return 0;
}

int
ovs_ctx_attr_enable_ha(ovs_ctx_attr_t *attr)
{
    if (attr == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    attr->enable_ha = true;
    return 0;
}

ovs_ctx_t*
ovs_ctx_new(const ovs_ctx_attr_t *attr)
{
    std::string uri;
    ovs_ctx_t *ctx = NULL;

    if(not attr)
    {
        errno = EINVAL;
        return NULL;
    }

    if (attr->transport != TransportType::SharedMemory &&
        attr->transport != TransportType::TCP &&
        attr->transport != TransportType::RDMA)
    {
        errno = EINVAL;
        return NULL;
    }

    switch (attr->transport)
    {
    case TransportType::TCP:
        uri = "tcp://" + attr->host + ":" + std::to_string(attr->port);
        break;
    case TransportType::RDMA:
        uri = "rdma://" + attr->host + ":" + std::to_string(attr->port);
        break;
    case TransportType::SharedMemory:
        break;
    case TransportType::Error: /* already catched */
        errno = EINVAL;
        return NULL;
    }

    const std::string loglevel_key("LIBOVSVOLUMEDRIVER_LOGLEVEL");
    const youtils::Severity severity(
            youtils::System::get_env_with_default(loglevel_key,
                                                  youtils::Severity::info));
    libvoldrv::Logger::ovs_log_init(severity);
    try
    {
        switch (attr->transport)
        {
        case TransportType::TCP:
        case TransportType::RDMA:
            ctx = new libvoldrv::NetworkHAContext(uri,
                                                  attr->network_qdepth,
                                                  attr->enable_ha,
                                                  default_request_callback);
            break;
        case TransportType::SharedMemory:
            ctx = new ShmContext;
            break;
        default:
            errno = EINVAL;
            return NULL;
        }
        ctx->transport = attr->transport;
        ctx->oflag = 0;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return NULL;
    }
    return ctx;
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

    if (not is_volume_name_valid(volume_name))
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
    ctx->oflag = oflag;
    return ctx->open_volume(volume_name, oflag);
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
    ctx->close_volume();
    delete ctx;
    return r;
}

int
ovs_create_volume(ovs_ctx_t *ctx,
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

    if ((ctx == NULL) or (not is_volume_name_valid(volume_name)))
    {
        errno = EINVAL;
        return (r = -1);
    }
    return (r = ctx->create_volume(volume_name, size));
}

int
ovs_remove_volume(ovs_ctx_t *ctx,
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

    if ((ctx == NULL) or (not is_volume_name_valid(volume_name)))
    {
        errno = EINVAL;
        return (r = -1);
    }
    return (r = ctx->remove_volume(volume_name));
}

int
ovs_truncate_volume(ovs_ctx_t *ctx,
                    const char *volume_name,
                    uint64_t length)
{
    int r = 0;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_truncate_volume_enter,
               volume_name,
               length);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_truncate_volume_exit,
                                          volume_name,
                                          r,
                                          errno);
                }));

    if ((ctx == NULL) or (not is_volume_name_valid(volume_name)))
    {
        errno = EINVAL;
        return (r = -1);
    }
    return (r = ctx->truncate_volume(volume_name, length));
}

int
ovs_snapshot_create(ovs_ctx_t *ctx,
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

    if ((ctx == NULL) or (not is_volume_name_valid(volume_name)))
    {
        errno = EINVAL;
        return (r = -1);
    }
    return (r = ctx->snapshot_create(volume_name,
                                     snapshot_name,
                                     timeout));
}

int
ovs_snapshot_rollback(ovs_ctx_t *ctx,
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

    if ((ctx == NULL) or (not is_volume_name_valid(volume_name)))
    {
        errno = EINVAL;
        return (r = -1);
    }
    return (r = ctx->snapshot_rollback(volume_name, snapshot_name));
}

int
ovs_snapshot_remove(ovs_ctx_t *ctx,
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

    if ((ctx == NULL) or (not is_volume_name_valid(volume_name)))
    {
        errno = EINVAL;
        return (r = -1);
    }

    return (r = ctx->snapshot_remove(volume_name,
                                     snapshot_name));
}

int
ovs_snapshot_list(ovs_ctx_t *ctx,
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

    if ((ctx == NULL) or (not is_volume_name_valid(volume_name)))
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
    ctx->list_snapshots(snaps,
                        volume_name,
                        &size,
                        &saved_errno);

    errno = saved_errno;
    if (saved_errno)
    {
        tracepoint(openvstorage_libovsvolumedriver,
                   ovs_snapshot_list_exit,
                   volume_name,
                   snap_list,
                   -1,
                   *max_snaps,
                   saved_errno);
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
ovs_snapshot_is_synced(ovs_ctx_t *ctx,
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

    if ((ctx == NULL) or (not is_volume_name_valid(volume_name)))
    {
        errno = EINVAL;
        return (r = -1);
    }
    return (r = ctx->is_snapshot_synced(volume_name,
                                        snapshot_name));
}

int
ovs_list_volumes(ovs_ctx_t *ctx,
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

    if (ctx == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }

    r = ctx->list_volumes(volumes);
    if (r < 0)
    {
        return r;
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
    int r = 0, accmode;

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

    accmode = ctx->oflag & O_ACCMODE;
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
    {
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
    }
        break;
    default:
        errno = EBADF;
        return -1;
    }

    ovs_aio_request *request;
    try
    {
        request = new ovs_aio_request(op,
                                      ovs_aiocbp,
                                      completion);
    }
    catch (const std::bad_alloc&)
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

    switch (op)
    {
    case RequestOp::Read:
    {
        /* on error returns -1, errno is already set */
        r = ctx->send_read_request(request,
                                   ovs_aiocbp);
    }
        break;
    case RequestOp::Write:
    {
        /* on error returns -1, errno is already set */
        r = ctx->send_write_request(request,
                                    ovs_aiocbp);
    }
        break;
    case RequestOp::Flush:
    case RequestOp::AsyncFlush:
    {
        r = ctx->send_flush_request(request);
    }
        break;
    default:
        errno = EINVAL; r = -1;
        break;
    }
    if (r < 0)
    {
        delete request;
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
                                   nullptr,
                                   RequestOp::Read);
}

int
ovs_aio_write(ovs_ctx_t *ctx,
              struct ovs_aiocb *ovs_aiocbp)
{
    return _ovs_submit_aio_request(ctx,
                                   ovs_aiocbp,
                                   nullptr,
                                   RequestOp::Write);
}

int
ovs_aio_error(ovs_ctx_t *ctx,
              struct ovs_aiocb *ovs_aiocbp)
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
        while (not ovs_aiocbp->request_->_signaled && r == 0)
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
    if (r == ETIMEDOUT)
    {
        r = -1;
        errno = EAGAIN;
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
    if (ctx == NULL)
    {
        errno = EINVAL;
        return nullptr;
    }

    ovs_buffer_t *buf = ctx->allocate(size);
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_allocate,
               ctx,
               reinterpret_cast<uintptr_t>(buf),
               size);
    return buf;
}

void*
ovs_buffer_data(ovs_buffer_t *ptr)
{
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_buffer_data,
               reinterpret_cast<uintptr_t>(ptr));

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
              reinterpret_cast<uintptr_t>(ptr));

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
    if (ctx == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    int r = ctx->deallocate(ptr);
    tracepoint(openvstorage_libovsvolumedriver,
               ovs_deallocate,
               ctx,
               reinterpret_cast<uintptr_t>(ptr),
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
        while (not completion->_signaled && r == 0)
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
    if (r == ETIMEDOUT)
    {
        r = -1;
        errno = EAGAIN;
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

    int r = _ovs_submit_aio_request(ctx,
                                    aio,
                                    completion,
                                    RequestOp::AsyncFlush);
    if (r < 0)
    {
        delete aio;
    }
    return r;
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
        (void) ovs_aio_finish(ctx, &aio);
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
        (void) ovs_aio_finish(ctx, &aio);
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
        tracepoint(openvstorage_libovsvolumedriver,
                   ovs_stat_exit,
                   ctx,
                   st,
                   -1,
                   0,
                   0);
        return -1;
    }

    int r = ctx->stat_volume(st);
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
                                     nullptr,
                                     RequestOp::Flush)) < 0)
    {
        return r;
    }

    if ((r = ovs_aio_suspend(ctx, &aio, NULL)) < 0)
    {
        (void) ovs_aio_finish(ctx, &aio);
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
ovs_truncate(ovs_ctx_t *ctx,
             uint64_t length)
{
    int r = 0, accmode;

    tracepoint(openvstorage_libovsvolumedriver,
               ovs_truncate_enter,
               length);

    auto on_exit(youtils::make_scope_exit([&]
                {
                    safe_errno_tracepoint(openvstorage_libovsvolumedriver,
                                          ovs_truncate_exit,
                                          r,
                                          errno);
                }));

    if (ctx == NULL)
    {
        errno = EINVAL;
        return (r = -1);
    }

    accmode = ctx->oflag & O_ACCMODE;
    if (accmode == O_RDONLY)
    {
        errno = EBADF;
        return -1;
    }
    return (r = ctx->truncate(length));
}
