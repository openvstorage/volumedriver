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

#ifndef NETWORK_XIO_HELPERS_H
#define NETWORK_XIO_HELPERS_H

static inline int
network_xio_create_context(ovs_ctx_t *ctx,
                           const char *volume_name)
{
    try
    {
        ctx->net_client_ =
            std::make_shared<volumedriverfs::NetworkXioClient>(ctx->uri,
                    ctx->net_client_qdepth);
        return ovs_xio_open_volume(ctx, volume_name);
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

static inline int
network_xio_create_volume(ovs_ctx_t *ctx,
                          const char *volume_name,
                          uint64_t size)
{
    int r;
    ovs_aio_request *request = create_new_request(RequestOp::Noop,
                                                  NULL,
                                                  NULL);
    if (request == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        volumedriverfs::NetworkXioClient::xio_create_volume(ctx->uri,
                                                            volume_name,
                                                            size,
                                                            static_cast<void*>(request));
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    delete request;
    return r;
}

static inline int
network_xio_remove_volume(ovs_ctx_t *ctx,
                          const char *volume_name)
{
    int r;
    ovs_aio_request *request = create_new_request(RequestOp::Noop,
                                                  NULL,
                                                  NULL);
    if (request == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        volumedriverfs::NetworkXioClient::xio_remove_volume(ctx->uri,
                                                            volume_name,
                                                            static_cast<void*>(request));
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    delete request;
    return r;
}

static inline int
network_xio_snapshot_create(ovs_ctx_t *ctx,
                            const char *volume_name,
                            const char *snapshot_name,
                            const int64_t timeout)
{
    int r;
    ovs_aio_request *request = create_new_request(RequestOp::Noop,
                                                  NULL,
                                                  NULL);
    if (request == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        volumedriverfs::NetworkXioClient::xio_create_snapshot(ctx->uri,
                                                              volume_name,
                                                              snapshot_name,
                                                              timeout,
                                                              static_cast<void*>(request));
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    delete request;
    return r;
}

static inline int
network_xio_snapshot_rollback(ovs_ctx_t *ctx,
                              const char *volume_name,
                              const char *snapshot_name)
{
    int r;
    ovs_aio_request *request = create_new_request(RequestOp::Noop,
                                                  NULL,
                                                  NULL);
    if (request == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        volumedriverfs::NetworkXioClient::xio_rollback_snapshot(ctx->uri,
                                                                volume_name,
                                                                snapshot_name,
                                                                static_cast<void*>(request));
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    delete request;
    return r;
}

static inline int
network_xio_snapshot_remove(ovs_ctx_t *ctx,
                            const char *volume_name,
                            const char *snapshot_name)
{
    int r;
    ovs_aio_request *request = create_new_request(RequestOp::Noop,
                                                  NULL,
                                                  NULL);
    if (request == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        volumedriverfs::NetworkXioClient::xio_delete_snapshot(ctx->uri,
                                                              volume_name,
                                                              snapshot_name,
                                                              static_cast<void*>(request));
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    delete request;
    return r;
}

static inline void
network_xio_list_snapshots(ovs_ctx_t *ctx,
                           std::vector<std::string>& snaps,
                           const char *volume_name,
                           uint64_t *size,
                           int *saved_errno)
{
    ovs_aio_request *request = create_new_request(RequestOp::Noop,
                                                  NULL,
                                                  NULL);
    if (request == NULL)
    {
        *saved_errno = ENOMEM;
        return;
    }
    try
    {
        volumedriverfs::NetworkXioClient::xio_list_snapshots(ctx->uri,
                                                             volume_name,
                                                             snaps,
                                                             size,
                                                             static_cast<void*>(request));
        *saved_errno = request->_errno;
    }
    catch (const std::bad_alloc&)
    {
        *saved_errno = ENOMEM;
    }
    catch (...)
    {
        *saved_errno = EIO;
    }
    delete request;
}

static inline int
network_xio_is_snapshot_synced(ovs_ctx_t *ctx,
                               const char *volume_name,
                               const char *snapshot_name)
{
    int r;
    ovs_aio_request *request = create_new_request(RequestOp::Noop,
                                                  NULL,
                                                  NULL);
    if (request == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        volumedriverfs::NetworkXioClient::xio_is_snapshot_synced(ctx->uri,
                                                                 volume_name,
                                                                 snapshot_name,
                                                                 static_cast<void*>(request));
        errno = request->_errno; r = request->_rv;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    delete request;
    return r;
}

static inline int
network_xio_list_volumes(ovs_ctx_t *ctx,
                         std::vector<std::string>& volumes)
{
    try
    {
        volumedriverfs::NetworkXioClient::xio_list_volumes(ctx->uri,
                                                           volumes);
        return 0;
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

static inline int
network_xio_send_read_request(ovs_ctx_t *ctx,
                              struct ovs_aiocb *ovs_aiocbp,
                              ovs_aio_request *request)
{
    int r = 0;
    try
    {
        ctx->net_client_->xio_send_read_request(ovs_aiocbp->aio_buf,
                                                ovs_aiocbp->aio_nbytes,
                                                ovs_aiocbp->aio_offset,
                                                reinterpret_cast<void*>(request));
    }
    catch (const volumedriverfs::XioClientQueueIsBusyException&)
    {
        errno = EBUSY;  r = -1;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

static inline int
network_xio_send_write_request(ovs_ctx_t *ctx,
                               struct ovs_aiocb *ovs_aiocbp,
                               ovs_aio_request *request)
{
    int r = 0;
    try
    {
        ctx->net_client_->xio_send_write_request(ovs_aiocbp->aio_buf,
                                                 ovs_aiocbp->aio_nbytes,
                                                 ovs_aiocbp->aio_offset,
                                                 reinterpret_cast<void*>(request));
    }
    catch (const volumedriverfs::XioClientQueueIsBusyException&)
    {
        errno = EBUSY;  r = -1;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

static inline int
network_xio_send_flush_request(ovs_ctx_t *ctx,
                               ovs_aio_request *request)
{
    int r = 0;
    try
    {
        ctx->net_client_->xio_send_flush_request(reinterpret_cast<void*>(request));
    }
    catch (const volumedriverfs::XioClientQueueIsBusyException&)
    {
        errno = EBUSY;  r = -1;
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    return r;
}

static inline int
network_xio_stat_volume(ovs_ctx_t *ctx, struct stat *st)
{
    int r;
    ovs_aio_request *request = create_new_request(RequestOp::Noop,
                                                  NULL,
                                                  NULL);
    if (request == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    try
    {
        volumedriverfs::NetworkXioClient::xio_stat_volume(ctx->uri,
                                                          ctx->volume_name,
                                                          static_cast<void*>(request));
        errno = request->_errno;
        r = request->_errno ? -1 : 0;
        if (not request->_errno)
        {
            /* 512 for now */
            st->st_blksize = 512;
            st->st_size = request->_rv;
        }
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM; r = -1;
    }
    catch (...)
    {
        errno = EIO; r = -1;
    }
    delete request;
    return r;
}

#endif //NETWORK_XIO_HELPERS_H
